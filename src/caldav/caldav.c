#include "caldav.h"

#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// -------------------- MACROS -------------------- //

#define DAV_NS    "DAV:"
#define CALDAV_NS "urn:ietf:params:xml:ns:caldav"
#define CS_NS     "http://calendarserver.org/ns/"
#define APPLE_NS  "http://apple.com/ns/ical/"

// -------------------- UTILS -------------------- //

static inline xmlNodePtr caldav_util_find_child_ns(xmlNodePtr parent, const gchar *name, const gchar *ns) {
  if (!parent) return NULL;
  for (xmlNodePtr node = parent->children; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, BAD_CAST name) && node->ns &&
        xmlStrEqual(node->ns->href, BAD_CAST ns))
      return node;
  }
  return NULL;
}

static inline gchar *caldav_util_get_node_content(xmlNodePtr node) {
  if (!node) return NULL;
  xmlChar *content = xmlNodeGetContent(node);
  if (!content) return NULL;
  gchar *result = g_strdup((const gchar *)content);
  xmlFree(content);
  return result;
}

static inline xmlNodePtr caldav_util_get_successful_prop(xmlNodePtr response, const gchar *name, const gchar *ns) {
  for (xmlNodePtr propstat = response->children; propstat; propstat = propstat->next) {
    if (propstat->type != XML_ELEMENT_NODE || !xmlStrEqual(propstat->name, BAD_CAST "propstat")) continue;
    xmlNodePtr status = caldav_util_find_child_ns(propstat, "status", DAV_NS);
    g_autofree gchar *status_text = caldav_util_get_node_content(status);
    if (!status_text || !strstr(status_text, " 200 ")) continue;
    xmlNodePtr prop = caldav_util_find_child_ns(propstat, "prop", DAV_NS);
    xmlNodePtr result = caldav_util_find_child_ns(prop, name, ns);
    if (result) return result;
  }
  return NULL;
}

static inline gboolean caldav_util_calendar_supports_vtodo(xmlNodePtr response) {
  xmlNodePtr resourcetype = caldav_util_get_successful_prop(response, "resourcetype", DAV_NS);
  if (!resourcetype) return FALSE;
  if (!caldav_util_find_child_ns(resourcetype, "calendar", CALDAV_NS)) return FALSE;
  xmlNodePtr supported = caldav_util_get_successful_prop(response, "supported-calendar-component-set", CALDAV_NS);
  if (!supported) return FALSE;
  for (xmlNodePtr comp = supported->children; comp; comp = comp->next) {
    if (comp->type != XML_ELEMENT_NODE || !xmlStrEqual(comp->name, BAD_CAST "comp")) continue;
    xmlChar *name = xmlGetProp(comp, BAD_CAST "name");
    gboolean matches = name && xmlStrEqual(name, BAD_CAST "VTODO");
    xmlFree(name);
    if (matches) return TRUE;
  }
  return FALSE;
}

static inline void caldav_util_add_basic_auth(SoupMessage *message, CalDAVClient *client) {
  g_autofree gchar *credentials =
      g_strdup_printf("%s:%s", caldav_client_get_username(client), caldav_client_get_password(client));
  g_autofree gchar *encoded = g_base64_encode((const guchar *)credentials, strlen(credentials));
  g_autofree gchar *authorization = g_strdup_printf("Basic %s", encoded);
  soup_message_headers_replace(soup_message_get_request_headers(message), "Authorization", authorization);
}

static inline GBytes *caldav_util_request(CalDAVClient *c, const char *method, const char *url, guint *status) {
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("DELETE", url);
  g_autoptr(GError) error = NULL;
  if (!message) {
    *status = SOUP_STATUS_NONE;
    return NULL;
  }
  caldav_util_add_basic_auth(message, c);
  GBytes *response = soup_session_send_and_read(session, message, NULL, &error);
  if (error) {
    g_warning("CalDAV: Request failed: %s", error->message);
    return NULL;
  }
  *status = soup_message_get_status(message);
  return response;
}

static inline gchar *caldav_util_pull_etag(CalDAVClient *c, SoupSession *session, const gchar *url) {
  const gchar *body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                      "<d:propfind xmlns:d=\"" DAV_NS "\">"
                      "  <d:prop><d:getetag/></d:prop>"
                      "</d:propfind>";

  g_autoptr(SoupMessage) message = soup_message_new("PROPFIND", url);
  g_autoptr(GBytes) request_body = g_bytes_new(body, strlen(body));
  g_autoptr(GError) error = NULL;
  soup_message_headers_replace(soup_message_get_request_headers(message), "Depth", "0");
  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Get ETag failed: %s", error->message);
    return NULL;
  }

  if (soup_message_get_status(message) != 207) {
    g_warning("CalDAV: Get ETag returned HTTP %u", soup_message_get_status(message));
    return NULL;
  }

  gsize size = 0;
  const gchar *data = g_bytes_get_data(response, &size);
  xmlDocPtr doc = xmlReadMemory(data, size, NULL, NULL, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  if (!doc) return NULL;

  gchar *etag = NULL;
  xmlNodePtr root = xmlDocGetRootElement(doc);
  for (xmlNodePtr node = root->children; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE || !xmlStrEqual(node->name, BAD_CAST "response")) continue;
    xmlNodePtr prop = caldav_util_get_successful_prop(node, "getetag", DAV_NS);
    if (prop) {
      etag = caldav_util_get_node_content(prop);
      break;
    }
  }
  xmlFreeDoc(doc);

  return etag;
}

// -------------------- CalDAVClient -------------------- //

struct _CalDAVClient {
  GObject parent_instance;

  // Properties
  gchar *url;
  gchar *username;
  gchar *password;
  gchar *principal;
  gchar *home_set;
};

G_DEFINE_TYPE(CalDAVClient, caldav_client, G_TYPE_OBJECT)

enum {
  CALDAV_CLIENT_PROP_0,

  CALDAV_CLIENT_PROP_URL,
  CALDAV_CLIENT_PROP_USERNAME,
  CALDAV_CLIENT_PROP_PASSWORD,
  CALDAV_CLIENT_PROP_PRINCIPAL,
  CALDAV_CLIENT_PROP_HOME_SET,

  CALDAV_CLIENT_N_PROPERTIES,
};

static GParamSpec *caldav_client_properties[CALDAV_CLIENT_N_PROPERTIES] = {NULL};

static void caldav_client_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  CalDAVClient *self = CALDAV_CLIENT(object);
  switch (prop_id) {
  case CALDAV_CLIENT_PROP_URL: g_value_set_string(value, self->url); break;
  case CALDAV_CLIENT_PROP_USERNAME: g_value_set_string(value, self->username); break;
  case CALDAV_CLIENT_PROP_PASSWORD: g_value_set_string(value, self->password); break;
  case CALDAV_CLIENT_PROP_PRINCIPAL: g_value_set_string(value, self->principal); break;
  case CALDAV_CLIENT_PROP_HOME_SET: g_value_set_string(value, self->principal); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void caldav_client_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  CalDAVClient *self = CALDAV_CLIENT(object);
  switch (prop_id) {
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void caldav_client_finalize(GObject *object) {
  CalDAVClient *self = CALDAV_CLIENT(object);
  g_clear_pointer(&self->url, g_free);
  g_clear_pointer(&self->username, g_free);
  g_clear_pointer(&self->password, g_free);
  g_clear_pointer(&self->principal, g_free);
  g_clear_pointer(&self->home_set, g_free);

  G_OBJECT_CLASS(caldav_client_parent_class)->finalize(object);
}

static void caldav_client_class_init(CalDAVClientClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = caldav_client_finalize;
  object_class->get_property = caldav_client_get_property;
  object_class->set_property = caldav_client_set_property;

  caldav_client_properties[CALDAV_CLIENT_PROP_URL] =
      g_param_spec_string("url", "Base URL", "User-provided base URL", NULL, G_PARAM_READABLE);
  caldav_client_properties[CALDAV_CLIENT_PROP_USERNAME] =
      g_param_spec_string("username", "Username", "Username", NULL, G_PARAM_READABLE);
  caldav_client_properties[CALDAV_CLIENT_PROP_PASSWORD] =
      g_param_spec_string("password", "Password", "Password", NULL, G_PARAM_READABLE);
  caldav_client_properties[CALDAV_CLIENT_PROP_PRINCIPAL] =
      g_param_spec_string("principal", "Principal URL", "Principal URL", NULL, G_PARAM_READABLE);
  caldav_client_properties[CALDAV_CLIENT_PROP_HOME_SET] =
      g_param_spec_string("home-set", "Home set URL", "Home set URL", NULL, G_PARAM_READABLE);

  g_object_class_install_properties(object_class, CALDAV_CLIENT_N_PROPERTIES, caldav_client_properties);
}

static void caldav_client_init(CalDAVClient *self) {}

static inline gchar *caldav_get_caldav_url(const gchar *base_url, CalDAVClient *c) {
  g_return_val_if_fail(base_url, NULL);

  // Always have a fallback URL.
  gchar *fallback = g_strdup(base_url);
  g_autoptr(GError) error = NULL;
  g_autoptr(GUri) base_uri = g_uri_parse(base_url, G_URI_FLAGS_NONE, &error);
  if (!base_uri) return fallback;

  g_autofree gchar *well_known_url = g_uri_join(G_URI_FLAGS_NONE, g_uri_get_scheme(base_uri), NULL, g_uri_get_host(base_uri),
                                                g_uri_get_port(base_uri), "/.well-known/caldav", NULL, NULL);
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("GET", well_known_url);
  if (!message) return fallback;

  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) goto fail;
  if (!SOUP_STATUS_IS_SUCCESSFUL(soup_message_get_status(message))) goto fail;
  return g_uri_to_string(soup_message_get_uri(message));

fail:
  g_warning("CalDAV: Autodiscovery failed: %s",
            error ? error->message : soup_status_get_phrase(soup_message_get_status(message)));

  return fallback;
}

static inline gchar *caldav_get_principal_url(CalDAVClient *c) {
  static const gchar *body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                             "<d:propfind xmlns:d=\"" DAV_NS "\">"
                             "  <d:prop><d:current-user-principal/></d:prop>"
                             "</d:propfind>";
  g_autoptr(GError) error = NULL;
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PROPFIND", caldav_client_get_url(c));
  if (!message) return NULL;

  soup_message_headers_replace(soup_message_get_request_headers(message), "Depth", "0");
  g_autoptr(GBytes) request_body = g_bytes_new_static(body, strlen(body));
  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Principal request failed: %s", error->message);
    return NULL;
  }

  if (soup_message_get_status(message) != SOUP_STATUS_MULTI_STATUS) {
    g_warning("CalDAV: Principal request returned HTTP %u", soup_message_get_status(message));
    return NULL;
  }

  gsize response_size;
  const gchar *response_data = g_bytes_get_data(response, &response_size);
  xmlDocPtr doc =
      xmlReadMemory(response_data, response_size, NULL, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!doc) return NULL;

  xmlXPathContextPtr xpath = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(xpath, BAD_CAST "d", BAD_CAST DAV_NS);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "//d:current-user-principal/d:href", xpath);
  gchar *principal = NULL;
  if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
    xmlNodePtr node = result->nodesetval->nodeTab[0];
    xmlChar *href = xmlNodeGetContent(node);
    if (href && href[0] != '\0')
      principal = g_uri_resolve_relative(caldav_client_get_url(c), (const gchar *)href, G_URI_FLAGS_NONE, &error);
    xmlFree(href);
  }
  if (!principal && error) g_warning("CalDAV: Cannot resolve principal URL: %s", error->message);
  if (result) xmlXPathFreeObject(result);
  xmlXPathFreeContext(xpath);
  xmlFreeDoc(doc);

  return principal;
}

static inline gchar *caldav_get_home_set_url(CalDAVClient *c) {
  static const gchar *body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                             "<d:propfind xmlns:d=\"" DAV_NS "\" xmlns:c=\"" CALDAV_NS "\">"
                             "  <d:prop>"
                             "    <c:calendar-home-set/>"
                             "  </d:prop>"
                             "</d:propfind>";
  g_autoptr(GError) error = NULL;
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PROPFIND", caldav_client_get_principal(c));
  if (!message) return NULL;

  soup_message_headers_replace(soup_message_get_request_headers(message), "Depth", "0");
  g_autoptr(GBytes) request_body = g_bytes_new_static(body, strlen(body));
  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Home-set request failed: %s", error->message);
    return NULL;
  }

  if (soup_message_get_status(message) != SOUP_STATUS_MULTI_STATUS) {
    g_warning("CalDAV: Home-set request returned HTTP %u", soup_message_get_status(message));
    return NULL;
  }

  gsize response_size;
  const gchar *response_data = g_bytes_get_data(response, &response_size);
  xmlDocPtr doc =
      xmlReadMemory(response_data, response_size, NULL, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!doc) return NULL;

  xmlXPathContextPtr xpath = xmlXPathNewContext(doc);
  if (!xpath) {
    xmlFreeDoc(doc);
    return NULL;
  }

  xmlXPathRegisterNs(xpath, BAD_CAST "d", BAD_CAST DAV_NS);
  xmlXPathRegisterNs(xpath, BAD_CAST "c", BAD_CAST CALDAV_NS);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "//c:calendar-home-set/d:href", xpath);
  gchar *home_set = NULL;
  if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
    xmlNodePtr node = result->nodesetval->nodeTab[0];
    xmlChar *href = xmlNodeGetContent(node);
    if (href && href[0] != '\0')
      home_set = g_uri_resolve_relative(caldav_client_get_principal(c), (const gchar *)href, G_URI_FLAGS_NONE, &error);
    xmlFree(href);
  }
  if (!home_set && error) g_warning("CalDAV: Cannot resolve home-set URL: %s", error->message);
  if (result) xmlXPathFreeObject(result);
  xmlXPathFreeContext(xpath);
  xmlFreeDoc(doc);

  return home_set;
}

CalDAVClient *caldav_client_new(const gchar *base_url, const gchar *username, const gchar *password) {
  g_return_val_if_fail(base_url, NULL);
  g_return_val_if_fail(username, NULL);
  g_return_val_if_fail(password, NULL);

  CalDAVClient *c = g_object_new(CALDAV_TYPE_CLIENT, NULL);
  c->username = g_strdup(username);
  c->password = g_strdup(password);
  c->url = caldav_get_caldav_url(base_url, c);
  c->principal = caldav_get_principal_url(c);
  if (!c->principal) {
    g_warning("CalDAV: Could not discover CalDAV principal");
    return NULL;
  }

  c->home_set = caldav_get_home_set_url(c);
  if (!c->principal) {
    g_warning("CalDAV: Could not get home set URL");
    return NULL;
  }

  return c;
}

GPtrArray *caldav_client_pull_calendars(CalDAVClient *self) {
  static const gchar *body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                             "<d:propfind xmlns:d=\"" DAV_NS "\" xmlns:c=\"" CALDAV_NS "\" "
                             "xmlns:cs=\"" CS_NS "\" xmlns:ical=\"" APPLE_NS "\">"
                             "  <d:prop>"
                             "    <d:displayname/>"
                             "    <d:resourcetype/>"
                             "    <c:supported-calendar-component-set/>"
                             "    <cs:getctag/>"
                             "    <ical:calendar-color/>"
                             "  </d:prop>"
                             "</d:propfind>";
  g_autoptr(GError) error = NULL;
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PROPFIND", self->home_set);
  if (!message) return NULL;

  soup_message_headers_replace(soup_message_get_request_headers(message), "Depth", "1");
  g_autoptr(GBytes) request_body = g_bytes_new_static(body, strlen(body));
  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, self);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Calendar list request failed: %s", error->message);
    return NULL;
  }

  if (soup_message_get_status(message) != SOUP_STATUS_MULTI_STATUS) {
    g_warning("CalDAV: Calendar list request returned HTTP %u", soup_message_get_status(message));
    return NULL;
  }

  gsize response_size;
  const gchar *response_data = g_bytes_get_data(response, &response_size);
  xmlDocPtr doc =
      xmlReadMemory(response_data, response_size, NULL, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!doc) return NULL;

  GPtrArray *calendars = g_ptr_array_new_with_free_func(g_object_unref);
  xmlNodePtr root = xmlDocGetRootElement(doc);
  for (xmlNodePtr node = root->children; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE || !xmlStrEqual(node->name, BAD_CAST "response") || !node->ns ||
        !xmlStrEqual(node->ns->href, BAD_CAST DAV_NS))
      continue;
    // Only include calendars supporting VTODO.
    if (!caldav_util_calendar_supports_vtodo(node)) continue;
    xmlNodePtr href_node = caldav_util_find_child_ns(node, "href", DAV_NS);
    g_autofree gchar *href = caldav_util_get_node_content(href_node);
    if (!href || !*href) continue;
    g_autofree gchar *url = g_uri_resolve_relative(self->home_set, href, G_URI_FLAGS_NONE, &error);
    if (!url) {
      g_warning("CalDAV: Cannot resolve calendar URL: %s", error->message);
      g_clear_error(&error);
      continue;
    }
    xmlNodePtr display_node = caldav_util_get_successful_prop(node, "displayname", DAV_NS);
    xmlNodePtr ctag_node = caldav_util_get_successful_prop(node, "getctag", CS_NS);
    xmlNodePtr color_node = caldav_util_get_successful_prop(node, "calendar-color", APPLE_NS);
    g_autofree gchar *display_name = caldav_util_get_node_content(display_node);
    g_autofree gchar *ctag = caldav_util_get_node_content(ctag_node);
    g_autofree gchar *color = caldav_util_get_node_content(color_node);
    CalDAVCalendar *calendar = caldav_calendar_new(url, display_name, color, ctag);
    g_ptr_array_add(calendars, calendar);
  }
  xmlFreeDoc(doc);

  return calendars;
}

const char *caldav_client_get_url(CalDAVClient *c) { return c->url; }
const char *caldav_client_get_username(CalDAVClient *c) { return c->username; }
const char *caldav_client_get_password(CalDAVClient *c) { return c->password; }
const char *caldav_client_get_principal(CalDAVClient *c) { return c->principal; }
const char *caldav_client_get_home_set(CalDAVClient *c) { return c->home_set; }

// ---------- CalDAVSyncResult ---------- //

CalDAVSyncResult *caldav_sync_result_new(void) {
  CalDAVSyncResult *self = g_new0(CalDAVSyncResult, 1);
  self->changed = g_ptr_array_new_with_free_func(g_object_unref);
  self->deleted_urls = g_ptr_array_new_with_free_func(g_free);
  return self;
}

void caldav_sync_result_free(CalDAVSyncResult *self) {
  if (!self) return;
  g_clear_pointer(&self->changed, g_ptr_array_unref);
  g_clear_pointer(&self->deleted_urls, g_ptr_array_unref);
  g_clear_pointer(&self->sync_token, g_free);
  g_clear_pointer(&self, g_free);
}

// -------------------- CalDAVCalendarChanges -------------------- //

static inline CalDAVCalendar *find_calendar(GPtrArray *calendars, const gchar *url) {
  for (guint i = 0; i < calendars->len; i++) {
    CalDAVCalendar *calendar = g_ptr_array_index(calendars, i);
    if (g_strcmp0(caldav_calendar_get_url(calendar), url) == 0) return calendar;
  }
  return NULL;
}

CalDAVCalendarChanges *caldav_calendar_changes_new(void) {
  CalDAVCalendarChanges *self = g_new0(CalDAVCalendarChanges, 1);

  self->added = g_ptr_array_new_with_free_func(g_object_unref);
  self->deleted = g_ptr_array_new_with_free_func(g_object_unref);
  self->properties_changed = g_ptr_array_new_with_free_func(g_object_unref);
  self->contents_changed = g_ptr_array_new_with_free_func(g_object_unref);

  return self;
}

void caldav_calendar_changes_free(CalDAVCalendarChanges *self) {
  if (!self) return;
  g_clear_pointer(&self->added, g_ptr_array_unref);
  g_clear_pointer(&self->properties_changed, g_ptr_array_unref);
  g_clear_pointer(&self->contents_changed, g_ptr_array_unref);
  g_clear_pointer(&self->deleted, g_ptr_array_unref);
  g_clear_pointer(&self, g_free);
}

// -------------------- CalDAVCalendar -------------------- //

struct _CalDAVCalendar {
  GObject parent_instance;

  gchar *url;
  gchar *ctag;
  gchar *display_name;
  gchar *color;
  gchar *sync_token;
};

G_DEFINE_TYPE(CalDAVCalendar, caldav_calendar, G_TYPE_OBJECT)

enum CalDAVCalendar {
  CALDAV_CALENDAR_PROP_0,

  CALDAV_CALENDAR_PROP_URL,
  CALDAV_CALENDAR_PROP_DISPLAY_NAME,
  CALDAV_CALENDAR_PROP_COLOR,
  CALDAV_CALENDAR_PROP_CTAG,

  CALDAV_CALENDAR_N_PROPERTIES,
};

static GParamSpec *caldav_calendar_properties[CALDAV_CALENDAR_N_PROPERTIES] = {NULL};

static void caldav_calendar_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  CalDAVCalendar *self = CALDAV_CALENDAR(object);

  switch (prop_id) {
  case CALDAV_CALENDAR_PROP_URL: g_value_set_string(value, self->url); break;
  case CALDAV_CALENDAR_PROP_DISPLAY_NAME: g_value_set_string(value, self->display_name); break;
  case CALDAV_CALENDAR_PROP_COLOR: g_value_set_string(value, self->color); break;
  case CALDAV_CALENDAR_PROP_CTAG: g_value_set_string(value, self->ctag); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void caldav_calendar_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  CalDAVCalendar *self = CALDAV_CALENDAR(object);
  switch (prop_id) {
  case CALDAV_CALENDAR_PROP_URL: {
    const gchar *s = g_value_get_string(value);
    g_clear_pointer(&self->url, g_free);
    self->url = g_strdup(s);
  } break;
  case CALDAV_CALENDAR_PROP_DISPLAY_NAME: {
    const gchar *s = g_value_get_string(value);
    g_clear_pointer(&self->display_name, g_free);
    self->display_name = g_strdup(s);
  } break;
  case CALDAV_CALENDAR_PROP_COLOR: {
    const gchar *s = g_value_get_string(value);
    g_clear_pointer(&self->color, g_free);
    self->color = g_strdup(s);
  } break;
  case CALDAV_CALENDAR_PROP_CTAG: {
    const gchar *s = g_value_get_string(value);
    g_clear_pointer(&self->ctag, g_free);
    self->ctag = g_strdup(s);
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void caldav_calendar_finalize(GObject *object) {
  CalDAVCalendar *self = CALDAV_CALENDAR(object);

  g_clear_pointer(&self->url, g_free);
  g_clear_pointer(&self->display_name, g_free);
  g_clear_pointer(&self->color, g_free);
  g_clear_pointer(&self->ctag, g_free);
  g_clear_pointer(&self->sync_token, g_free);

  G_OBJECT_CLASS(caldav_calendar_parent_class)->finalize(object);
}

static void caldav_calendar_class_init(CalDAVCalendarClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = caldav_calendar_get_property;
  object_class->set_property = caldav_calendar_set_property;
  object_class->finalize = caldav_calendar_finalize;

  caldav_calendar_properties[CALDAV_CALENDAR_PROP_URL] =
      g_param_spec_string("url", "URL", "Calendar URL", NULL, G_PARAM_READABLE);
  caldav_calendar_properties[CALDAV_CALENDAR_PROP_DISPLAY_NAME] =
      g_param_spec_string("display-name", "Display name", "Calendar display name", NULL, G_PARAM_READWRITE);
  caldav_calendar_properties[CALDAV_CALENDAR_PROP_COLOR] =
      g_param_spec_string("color", "Color", "Calendar color", NULL, G_PARAM_READWRITE);
  caldav_calendar_properties[CALDAV_CALENDAR_PROP_CTAG] =
      g_param_spec_string("ctag", "CTag", "Calendar synchronization tag", NULL, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, CALDAV_CALENDAR_N_PROPERTIES, caldav_calendar_properties);
}

static void caldav_calendar_init(CalDAVCalendar *self) {}

// ---------- PRIVATE ---------- //

static inline gchar *caldav_pull_ctag(CalDAVClient *c, const gchar *calendar_url) {
  const gchar *body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                      "<d:propfind xmlns:d=\"" DAV_NS "\" "
                      "xmlns:cs=\"" CS_NS "\">"
                      "<d:prop><cs:getctag/></d:prop>"
                      "</d:propfind>";

  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PROPFIND", calendar_url);
  g_autoptr(GBytes) request_body = g_bytes_new(body, strlen(body));
  g_autoptr(GError) error = NULL;
  soup_message_headers_replace(soup_message_get_request_headers(message), "Depth", "0");
  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response || soup_message_get_status(message) != 207) return NULL;

  gsize size;
  const gchar *data = g_bytes_get_data(response, &size);
  xmlDocPtr doc = xmlReadMemory(data, size, NULL, NULL, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  if (!doc) return NULL;

  xmlXPathContextPtr xpath = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(xpath, BAD_CAST "cs", BAD_CAST CS_NS);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "//cs:getctag", xpath);
  gchar *ctag = NULL;
  if (result && result->nodesetval && result->nodesetval->nodeNr > 0)
    ctag = caldav_util_get_node_content(result->nodesetval->nodeTab[0]);
  if (result) xmlXPathFreeObject(result);
  xmlXPathFreeContext(xpath);
  xmlFreeDoc(doc);

  return ctag;
}

static inline guint caldav_send_create_calendar_request(CalDAVClient *c, SoupSession *session, const gchar *method,
                                                        const gchar *url, const gchar *body) {
  g_autoptr(SoupMessage) message = soup_message_new(method, url);
  g_autoptr(GBytes) request_body = g_bytes_new(body, strlen(body));
  g_autoptr(GError) error = NULL;

  if (!message) return 0;

  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: %s request failed: %s", method, error->message);
    return 0;
  }

  return soup_message_get_status(message);
}

// ---------- PUBLIC ---------- //

CalDAVCalendar *caldav_calendar_new(const gchar *url, const gchar *display_name, const gchar *color, const gchar *ctag) {
  CalDAVCalendar *self = g_object_new(CALDAV_TYPE_CALENDAR, NULL);

  self->url = g_strdup(url);
  self->display_name = g_strdup(display_name);
  self->color = g_strdup(color);
  self->ctag = g_strdup(ctag);

  return self;
}

CalDAVCalendar *caldav_calendar_create(CalDAVClient *c, const gchar *display_name, const gchar *color) {
  g_return_val_if_fail(CALDAV_IS_CLIENT(c), NULL);

  const gchar *home_set_url = caldav_client_get_home_set(c);

  g_return_val_if_fail(home_set_url && *home_set_url, NULL);
  g_return_val_if_fail(display_name && *display_name, NULL);

  // Generate calendar URL
  g_autofree gchar *uuid = g_uuid_string_random();
  g_autofree gchar *collection_url =
      g_str_has_suffix(home_set_url, "/") ? g_strdup(home_set_url) : g_strconcat(home_set_url, "/", NULL);
  g_autofree gchar *calendar_url = g_uri_resolve_relative(collection_url, uuid, G_URI_FLAGS_NONE, NULL);
  if (!calendar_url) return NULL;

  // Escape XML values
  g_autofree gchar *escaped_name = g_markup_escape_text(display_name, -1);
  g_autofree gchar *escaped_color = color ? g_markup_escape_text(color, -1) : NULL;
  g_autofree gchar *color_prop =
      escaped_color ? g_strdup_printf("<a:calendar-color>%s</a:calendar-color>", escaped_color) : g_strdup("");

  // Standard MKCALENDAR body
  g_autofree gchar *mkcalendar_body = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                                                      "<c:mkcalendar xmlns:d=\"" DAV_NS "\" "
                                                      "xmlns:c=\"" CALDAV_NS "\" "
                                                      "xmlns:a=\"" APPLE_NS "\">"
                                                      "<d:set><d:prop>"
                                                      "<d:resourcetype><d:collection/><c:calendar/></d:resourcetype>"
                                                      "<d:displayname>%s</d:displayname>"
                                                      "<c:supported-calendar-component-set>"
                                                      "<c:comp name=\"VTODO\"/>"
                                                      "</c:supported-calendar-component-set>"
                                                      "%s"
                                                      "</d:prop></d:set>"
                                                      "</c:mkcalendar>",
                                                      escaped_name, color_prop);

  // Radicale-compatible MKCOL body
  g_autofree gchar *mkcol_body = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                                                 "<d:create xmlns:d=\"" DAV_NS "\" "
                                                 "xmlns:c=\"" CALDAV_NS "\" "
                                                 "xmlns:a=\"" APPLE_NS "\">"
                                                 "<d:set><d:prop>"
                                                 "<d:resourcetype><d:collection/><c:calendar/></d:resourcetype>"
                                                 "<d:displayname>%s</d:displayname>"
                                                 "<c:supported-calendar-component-set>"
                                                 "<c:comp name=\"VTODO\"/>"
                                                 "</c:supported-calendar-component-set>"
                                                 "%s"
                                                 "</d:prop></d:set>"
                                                 "</d:create>",
                                                 escaped_name, color_prop);

  g_autoptr(SoupSession) session = soup_session_new();
  // Try standard CalDAV method first
  guint status = caldav_send_create_calendar_request(c, session, "MKCALENDAR", calendar_url, mkcalendar_body);
  // Fall back to MKCOL (Radicale compatibility)
  if (status == SOUP_STATUS_METHOD_NOT_ALLOWED || status == SOUP_STATUS_NOT_IMPLEMENTED)
    status = caldav_send_create_calendar_request(c, session, "MKCOL", calendar_url, mkcol_body);
  if (status != SOUP_STATUS_CREATED) {
    g_warning("CalDAV: Create calendar returned HTTP %u", status);
    return NULL;
  }

  g_autofree gchar *ctag = caldav_pull_ctag(c, calendar_url);

  return caldav_calendar_new(calendar_url, display_name, color, ctag);
}

gboolean caldav_calendar_delete(CalDAVClient *c, CalDAVCalendar *calendar) {
  g_return_val_if_fail(CALDAV_IS_CLIENT(c), FALSE);
  g_return_val_if_fail(CALDAV_IS_CALENDAR(calendar), FALSE);
  const gchar *url = caldav_calendar_get_url(calendar);
  g_return_val_if_fail(url && *url, FALSE);
  guint status;
  g_autoptr(GBytes) response = caldav_util_request(c, "DELETE", url, &status);
  if (!response) return FALSE;
  if (status != SOUP_STATUS_OK && status != SOUP_STATUS_NO_CONTENT && status != SOUP_STATUS_ACCEPTED) {
    g_warning("CalDAV: Delete calendar returned HTTP %u", status);
    return FALSE;
  }
  return TRUE;
}

gboolean caldav_calendar_update(CalDAVClient *c, CalDAVCalendar *calendar, const gchar *display_name, const gchar *color) {
  g_return_val_if_fail(CALDAV_IS_CLIENT(c), FALSE);
  g_return_val_if_fail(CALDAV_IS_CALENDAR(calendar), FALSE);

  const gchar *url = caldav_calendar_get_url(calendar);
  g_return_val_if_fail(url && *url, FALSE);

  g_autofree gchar *escaped_name = display_name ? g_markup_escape_text(display_name, -1) : NULL;
  g_autofree gchar *escaped_color = color ? g_markup_escape_text(color, -1) : NULL;
  GString *properties = g_string_new(NULL);
  if (display_name) g_string_append_printf(properties, "<d:displayname>%s</d:displayname>", escaped_name);
  if (color) g_string_append_printf(properties, "<a:calendar-color>%s</a:calendar-color>", escaped_color);
  if (!properties->len) {
    g_string_free(properties, TRUE);
    return TRUE;
  }

  g_autofree gchar *body = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                                           "<d:propertyupdate xmlns:d=\"" DAV_NS "\" xmlns:a=\"" APPLE_NS "\">"
                                           "<d:set><d:prop>%s</d:prop></d:set>"
                                           "</d:propertyupdate>",
                                           properties->str);
  g_string_free(properties, TRUE);
  g_autoptr(GBytes) request_body = g_bytes_new(body, strlen(body));
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PROPPATCH", url);
  g_autoptr(GError) error = NULL;
  if (!message) return FALSE;

  soup_message_set_request_body_from_bytes(message, "application/xml; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Update calendar request failed: %s", error->message);
    return FALSE;
  }

  if (soup_message_get_status(message) != SOUP_STATUS_MULTI_STATUS) {
    g_warning("CalDAV: Update calendar returned HTTP %u", soup_message_get_status(message));
    return FALSE;
  }

  if (display_name) caldav_calendar_set_display_name(calendar, display_name);
  if (color) caldav_calendar_set_color(calendar, color);
  g_autofree gchar *ctag = caldav_pull_ctag(c, url);
  if (ctag) caldav_calendar_set_ctag(calendar, ctag);

  return TRUE;
}

CalDAVSyncResult *caldav_calendar_sync(CalDAVClient *c, CalDAVCalendar *calendar) {
  g_assert(CALDAV_IS_CLIENT(c));
  g_assert(CALDAV_IS_CALENDAR(calendar));

  const gchar *calendar_url = caldav_calendar_get_url(calendar);
  const gchar *sync_token = caldav_calendar_get_sync_token(calendar);
  g_autofree gchar *escaped_token = g_markup_escape_text(sync_token ? sync_token : "", -1);
  g_autofree gchar *body = g_strdup_printf("<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                                           "<d:sync-collection xmlns:d=\"" DAV_NS "\" "
                                           "xmlns:c=\"" CALDAV_NS "\">"
                                           "<d:sync-token>%s</d:sync-token>"
                                           "<d:sync-level>1</d:sync-level>"
                                           "<d:prop>"
                                           "<d:getetag/>"
                                           "<c:calendar-data/>"
                                           "</d:prop>"
                                           "</d:sync-collection>",
                                           escaped_token);
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("REPORT", calendar_url);
  caldav_util_add_basic_auth(message, c);
  soup_message_headers_replace(soup_message_get_request_headers(message), "Depth", "0");
  soup_message_set_request_body(message, "application/xml; charset=utf-8",
                                g_memory_input_stream_new_from_data(g_strdup(body), strlen(body), g_free), -1);
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Sync request failed: %s", error->message);
    return NULL;
  }

  guint status = soup_message_get_status(message);
  if (status != SOUP_STATUS_MULTI_STATUS) {
    g_warning("CalDAV: Sync request failed with HTTP %u", status);
    return NULL;
  }

  gsize response_size = 0;
  const gchar *response_data = g_bytes_get_data(response, &response_size);
  xmlDocPtr doc = xmlReadMemory(response_data, response_size, NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NONET);
  if (!doc) {
    g_warning("CalDAV: Failed to parse sync response");
    return NULL;
  }

  CalDAVSyncResult *result = caldav_sync_result_new();
  xmlNodePtr root = xmlDocGetRootElement(doc);
  // Get the new sync token
  xmlNodePtr token_node = caldav_util_find_child_ns(root, "sync-token", DAV_NS);
  if (token_node) result->sync_token = caldav_util_get_node_content(token_node);
  // Process changed and deleted resources.
  for (xmlNodePtr node = root->children; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE || !xmlStrEqual(node->name, BAD_CAST "response") ||
        !xmlStrEqual(node->ns->href, BAD_CAST DAV_NS))
      continue;
    g_autofree gchar *href = NULL;
    g_autofree gchar *url = NULL;
    xmlNodePtr href_node = caldav_util_find_child_ns(node, "href", DAV_NS);
    if (!href_node) continue;
    href = caldav_util_get_node_content(href_node);
    if (!href || !*href) continue;
    g_autoptr(GError) uri_error = NULL;
    url = g_uri_resolve_relative(calendar_url, href, G_URI_FLAGS_NONE, &uri_error);
    if (!url) {
      g_warning("CalDAV: Failed to resolve resource URL: %s", uri_error->message);
      continue;
    }
    // Deleted resource.
    xmlNodePtr status_node = caldav_util_find_child_ns(node, "status", DAV_NS);
    if (status_node) {
      g_autofree gchar *status_text = caldav_util_get_node_content(status_node);
      if (status_text && strstr(status_text, " 404 ")) g_ptr_array_add(result->deleted_urls, g_steal_pointer(&url));
      continue;
    }
    // Changed or newly created resource.
    xmlNodePtr etag_node = caldav_util_get_successful_prop(node, "getetag", DAV_NS);
    xmlNodePtr data_node = caldav_util_get_successful_prop(node, "calendar-data", CALDAV_NS);
    if (!data_node) continue;
    g_autofree gchar *etag = etag_node ? caldav_util_get_node_content(etag_node) : NULL;
    g_autofree gchar *ical_data = caldav_util_get_node_content(data_node);
    if (!ical_data) continue;
    icalcomponent *vcalendar = icalparser_parse_string(ical_data);
    if (!vcalendar) continue;
    if (icalcomponent_isa(vcalendar) != ICAL_VCALENDAR_COMPONENT) {
      icalcomponent_free(vcalendar);
      continue;
    }
    for (icalcomponent *todo = icalcomponent_get_first_component(vcalendar, ICAL_VTODO_COMPONENT); todo;
         todo = icalcomponent_get_next_component(vcalendar, ICAL_VTODO_COMPONENT)) {
      icalcomponent *clone = icalcomponent_clone(todo);
      CalDAVTodo *item = caldav_todo_new(clone, url, etag);
      if (item) g_ptr_array_add(result->changed, item);
    }
    icalcomponent_free(vcalendar);
  }
  xmlFreeDoc(doc);
  if (!result->sync_token) {
    g_warning("CalDAV: Sync response did not contain a sync token");
    caldav_sync_result_free(result);
    return NULL;
  }

  caldav_calendar_set_sync_token(calendar, result->sync_token);

  return result;
}

CalDAVTodo *caldav_calendar_create_todo(CalDAVClient *c, CalDAVCalendar *calendar, icalcomponent *todo) {
  g_return_val_if_fail(CALDAV_IS_CLIENT(c), NULL);
  g_return_val_if_fail(CALDAV_IS_CALENDAR(calendar), NULL);
  g_return_val_if_fail(todo != NULL, NULL);
  g_return_val_if_fail(icalcomponent_isa(todo) == ICAL_VTODO_COMPONENT, NULL);

  const gchar *uid = icalcomponent_get_uid(todo);
  g_return_val_if_fail(uid && *uid, NULL);

  // Generate resource URL
  g_autofree gchar *escaped_uid = g_uri_escape_string(uid, NULL, FALSE);
  g_autofree gchar *resource_name = g_strconcat(escaped_uid, ".ics", NULL);
  const gchar *calendar_url = caldav_calendar_get_url(calendar);
  g_autofree gchar *collection_url =
      g_str_has_suffix(calendar_url, "/") ? g_strdup(calendar_url) : g_strconcat(calendar_url, "/", NULL);
  g_autoptr(GError) error = NULL;
  g_autofree gchar *todo_url = g_uri_resolve_relative(collection_url, resource_name, G_URI_FLAGS_NONE, &error);
  if (!todo_url) {
    g_warning("CalDAV: Cannot generate VTODO URL: %s", error->message);
    return NULL;
  }

  // Create VCALENDAR wrapper
  icalcomponent *vcalendar = icalcomponent_new_vcalendar();
  icalcomponent_add_property(vcalendar, icalproperty_new_version("2.0"));
  icalcomponent_add_property(vcalendar, icalproperty_new_prodid("-//libcaldav//EN"));
  icalcomponent_add_component(vcalendar, icalcomponent_clone(todo));
  gchar *ical_data = icalcomponent_as_ical_string_r(vcalendar);
  icalcomponent_free(vcalendar);
  if (!ical_data) return NULL;

  g_autoptr(GBytes) request_body = g_bytes_new(ical_data, strlen(ical_data));
  icalmemory_free_buffer(ical_data);
  // Create request
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PUT", todo_url);
  if (!message) return NULL;

  soup_message_headers_replace(soup_message_get_request_headers(message), "Content-Type", "text/calendar; charset=utf-8");
  // Do not overwrite an existing resource
  soup_message_headers_replace(soup_message_get_request_headers(message), "If-None-Match", "*");
  soup_message_set_request_body_from_bytes(message, "text/calendar; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Create VTODO request failed: %s", error->message);
    return NULL;
  }

  guint status = soup_message_get_status(message);
  if (status != SOUP_STATUS_CREATED && status != SOUP_STATUS_NO_CONTENT) {
    g_warning("CalDAV: Create VTODO returned HTTP %u", status);
    return NULL;
  }

  const gchar *etag = soup_message_headers_get_one(soup_message_get_response_headers(message), "ETag");
  g_autofree gchar *new_etag = g_strdup(etag);
  if (!new_etag) new_etag = caldav_util_pull_etag(c, session, todo_url);

  return caldav_todo_new(icalcomponent_clone(todo), todo_url, etag);
}

CalDAVCalendarChanges *caldav_calendar_compare(GPtrArray *old_calendars, GPtrArray *new_calendars) {
  g_return_val_if_fail(old_calendars != NULL, NULL);
  g_return_val_if_fail(new_calendars != NULL, NULL);

  CalDAVCalendarChanges *changes = caldav_calendar_changes_new();
  // Added and changed calendars.
  for (guint i = 0; i < new_calendars->len; i++) {
    CalDAVCalendar *new_calendar = g_ptr_array_index(new_calendars, i);
    CalDAVCalendar *old_calendar = find_calendar(old_calendars, caldav_calendar_get_url(new_calendar));
    if (!old_calendar) {
      g_ptr_array_add(changes->added, g_object_ref(new_calendar));
      continue;
    }
    // Preserve the existing sync token.
    caldav_calendar_set_sync_token(new_calendar, caldav_calendar_get_sync_token(old_calendar));
    gboolean properties_changed =
        g_strcmp0(caldav_calendar_get_display_name(old_calendar), caldav_calendar_get_display_name(new_calendar)) != 0 ||
        g_strcmp0(caldav_calendar_get_color(old_calendar), caldav_calendar_get_color(new_calendar)) != 0;
    if (properties_changed) g_ptr_array_add(changes->properties_changed, g_object_ref(new_calendar));
    gboolean contents_changed = g_strcmp0(caldav_calendar_get_ctag(old_calendar), caldav_calendar_get_ctag(new_calendar)) != 0;
    if (contents_changed) g_ptr_array_add(changes->contents_changed, g_object_ref(new_calendar));
  }
  // Deleted calendars.
  for (guint i = 0; i < old_calendars->len; i++) {
    CalDAVCalendar *old_calendar = g_ptr_array_index(old_calendars, i);
    if (!find_calendar(new_calendars, caldav_calendar_get_url(old_calendar))) {
      g_ptr_array_add(changes->deleted, g_object_ref(old_calendar));
    }
  }
  return changes;
}

// ---------- PROPERTIES ---------- //

const gchar *caldav_calendar_get_url(CalDAVCalendar *self) {
  g_return_val_if_fail(CALDAV_IS_CALENDAR(self), NULL);
  return self->url;
}

const gchar *caldav_calendar_get_display_name(CalDAVCalendar *self) {
  g_return_val_if_fail(CALDAV_IS_CALENDAR(self), NULL);
  return self->display_name;
}

const gchar *caldav_calendar_get_ctag(CalDAVCalendar *self) {
  g_return_val_if_fail(CALDAV_IS_CALENDAR(self), NULL);
  return self->ctag;
}

const gchar *caldav_calendar_get_color(CalDAVCalendar *self) {
  g_return_val_if_fail(CALDAV_IS_CALENDAR(self), NULL);
  return self->color;
}

const gchar *caldav_calendar_get_sync_token(CalDAVCalendar *self) {
  g_return_val_if_fail(CALDAV_IS_CALENDAR(self), NULL);
  return self->sync_token;
}

void caldav_calendar_set_display_name(CalDAVCalendar *self, const gchar *display_name) {
  g_object_set(self, "display-name", display_name, NULL);
}

void caldav_calendar_set_color(CalDAVCalendar *self, const gchar *color) { g_object_set(self, "color", color, NULL); }

void caldav_calendar_set_ctag(CalDAVCalendar *self, const gchar *ctag) { g_object_set(self, "ctag", ctag, NULL); }

void caldav_calendar_set_sync_token(CalDAVCalendar *self, const gchar *sync_token) {
  g_return_if_fail(CALDAV_IS_CALENDAR(self));
  if (g_strcmp0(self->sync_token, sync_token) == 0) return;
  g_free(self->sync_token);
  self->sync_token = g_strdup(sync_token);
}

// -------------------- CalDAVTodo -------------------- //

struct _CalDAVTodo {
  GObject parent_instance;

  icalcomponent *ical;
  gchar *url;
  gchar *etag;
};

G_DEFINE_TYPE(CalDAVTodo, caldav_todo, G_TYPE_OBJECT)

enum {
  CALDAV_TODO_PROP_0,

  CALDAV_TODO_PROP_ICAL,
  CALDAV_TODO_PROP_URL,
  CALDAV_TODO_PROP_ETAG,

  CALDAV_TODO_N_PROPERTIES,
};

static GParamSpec *caldav_todo_properties[CALDAV_TODO_N_PROPERTIES];

static void caldav_todo_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  CalDAVTodo *self = CALDAV_TODO(object);

  switch (prop_id) {
  case CALDAV_TODO_PROP_ICAL: g_value_set_pointer(value, self->ical); break;
  case CALDAV_TODO_PROP_URL: g_value_set_string(value, self->url); break;
  case CALDAV_TODO_PROP_ETAG: g_value_set_string(value, self->etag); break;

  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void caldav_todo_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  CalDAVTodo *self = CALDAV_TODO(object);
  switch (prop_id) {
  case CALDAV_TODO_PROP_ICAL: {
    icalcomponent *ical = g_value_get_pointer(value);
    g_clear_pointer(&self->ical, icalcomponent_free);
    self->ical = ical;
  } break;
  case CALDAV_TODO_PROP_URL: {
    const gchar *s = g_value_get_string(value);
    g_clear_pointer(&self->url, g_free);
    self->url = g_strdup(s);
  } break;
  case CALDAV_TODO_PROP_ETAG: {
    const gchar *s = g_value_get_string(value);
    g_clear_pointer(&self->etag, g_free);
    self->etag = g_strdup(s);
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void caldav_todo_finalize(GObject *object) {
  CalDAVTodo *self = CALDAV_TODO(object);

  g_clear_pointer(&self->ical, icalcomponent_free);
  g_clear_pointer(&self->url, g_free);
  g_clear_pointer(&self->etag, g_free);

  G_OBJECT_CLASS(caldav_todo_parent_class)->finalize(object);
}

static void caldav_todo_class_init(CalDAVTodoClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = caldav_todo_get_property;
  object_class->set_property = caldav_todo_set_property;
  object_class->finalize = caldav_todo_finalize;

  caldav_todo_properties[CALDAV_TODO_PROP_ICAL] =
      g_param_spec_string("ical", "ICal", "iCalendar component", NULL, G_PARAM_READWRITE);
  caldav_todo_properties[CALDAV_TODO_PROP_URL] = g_param_spec_string("url", "URL", "Remote VTODO URL", NULL, G_PARAM_READWRITE);
  caldav_todo_properties[CALDAV_TODO_PROP_ETAG] =
      g_param_spec_string("etag", "ETag", "Remote VTODO ETag", NULL, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, CALDAV_TODO_N_PROPERTIES, caldav_todo_properties);
}

static void caldav_todo_init(CalDAVTodo *self) {}

CalDAVTodo *caldav_todo_new(icalcomponent *ical, const gchar *url, const gchar *etag) {
  g_return_val_if_fail(ical != NULL, NULL);
  g_return_val_if_fail(icalcomponent_isa(ical) == ICAL_VTODO_COMPONENT, NULL);

  CalDAVTodo *self = g_object_new(CALDAV_TYPE_TODO, NULL);

  self->ical = ical;
  self->url = g_strdup(url);
  self->etag = g_strdup(etag);

  return self;
}

gboolean caldav_todo_delete(CalDAVClient *c, CalDAVTodo *self) {
  g_return_val_if_fail(CALDAV_IS_CLIENT(c), FALSE);
  g_return_val_if_fail(CALDAV_IS_TODO(self), FALSE);

  const gchar *url = caldav_todo_get_url(self);
  const gchar *etag = caldav_todo_get_etag(self);

  g_return_val_if_fail(url && *url, FALSE);

  if (!etag || !*etag) {
    g_warning("CalDAV: Cannot delete VTODO without ETag");
    return FALSE;
  }

  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("DELETE", url);
  g_autoptr(GError) error = NULL;
  if (!message) return FALSE;

  // soup_message_headers_replace(soup_message_get_request_headers(message), "If-Match", etag);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Delete VTODO request failed: %s", error->message);
    return FALSE;
  }

  guint status = soup_message_get_status(message);
  // if (status == SOUP_STATUS_PRECONDITION_FAILED) {
  //   g_warning("CalDAV: VTODO was modified remotely (ETag mismatch)");
  //   return FALSE;
  // }

  if (status != SOUP_STATUS_OK && status != SOUP_STATUS_NO_CONTENT && status != SOUP_STATUS_ACCEPTED) {
    g_warning("CalDAV: Delete VTODO returned HTTP %u", status);
    return FALSE;
  }

  return TRUE;
}

CalDAVTodo *caldav_todo_update(CalDAVClient *c, CalDAVTodo *self, icalcomponent *todo) {
  const gchar *url = caldav_todo_get_url(self);
  const gchar *etag = caldav_todo_get_etag(self);

  // Create VCALENDAR wrapper
  icalcomponent *vcalendar = icalcomponent_new_vcalendar();
  icalcomponent_add_property(vcalendar, icalproperty_new_version("2.0"));
  icalcomponent_add_property(vcalendar, icalproperty_new_prodid("-//libcaldav//EN"));
  icalcomponent_add_component(vcalendar, icalcomponent_clone(todo));
  gchar *ical_data = icalcomponent_as_ical_string_r(vcalendar);
  icalcomponent_free(vcalendar);
  if (!ical_data) return NULL;

  g_autoptr(GBytes) request_body = g_bytes_new(ical_data, strlen(ical_data));
  icalmemory_free_buffer(ical_data);
  // Update request
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PUT", url);
  g_autoptr(GError) error = NULL;
  if (!message) return NULL;

  soup_message_headers_replace(soup_message_get_request_headers(message), "Content-Type", "text/calendar; charset=utf-8");
  // Only update if the server version hasn't changed
  soup_message_headers_replace(soup_message_get_request_headers(message), "If-Match", etag);
  soup_message_set_request_body_from_bytes(message, "text/calendar; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Update VTODO request failed: %s", error->message);
    return NULL;
  }

  guint status = soup_message_get_status(message);
  if (status == SOUP_STATUS_PRECONDITION_FAILED) {
    g_warning("CalDAV: VTODO was modified remotely (ETag mismatch)");
    return NULL;
  }

  if (status != SOUP_STATUS_OK && status != SOUP_STATUS_NO_CONTENT) {
    g_warning("CalDAV: Update VTODO returned HTTP %u", status);
    return NULL;
  }

  // Get updated ETag
  const gchar *response_etag = soup_message_headers_get_one(soup_message_get_response_headers(message), "ETag");
  g_autofree gchar *new_etag = g_strdup(response_etag);
  if (!new_etag) new_etag = caldav_util_pull_etag(c, session, url);

  return caldav_todo_new(icalcomponent_clone(todo), url, new_etag);
}

// ---------- PROPERTIES ---------- //

icalcomponent *caldav_todo_get_ical(CalDAVTodo *self) {
  g_return_val_if_fail(CALDAV_IS_TODO(self), NULL);
  return self->ical;
}

const gchar *caldav_todo_get_url(CalDAVTodo *self) {
  g_return_val_if_fail(CALDAV_IS_TODO(self), NULL);
  return self->url;
}

const gchar *caldav_todo_get_etag(CalDAVTodo *self) {
  g_return_val_if_fail(CALDAV_IS_TODO(self), NULL);
  return self->etag;
}

gboolean caldav_todo_set_ical(CalDAVTodo *self, CalDAVClient *c, icalcomponent *ical) {
  const gchar *url = caldav_todo_get_url(self);
  const gchar *etag = caldav_todo_get_etag(self);

  // Create VCALENDAR wrapper
  icalcomponent *vcalendar = icalcomponent_new_vcalendar();
  icalcomponent_add_property(vcalendar, icalproperty_new_version("2.0"));
  icalcomponent_add_property(vcalendar, icalproperty_new_prodid("-//libcaldav//EN"));
  icalcomponent_add_component(vcalendar, icalcomponent_clone(ical));
  gchar *ical_data = icalcomponent_as_ical_string_r(vcalendar);
  icalcomponent_free(vcalendar);
  if (!ical_data) return FALSE;

  g_autoptr(GBytes) request_body = g_bytes_new(ical_data, strlen(ical_data));
  icalmemory_free_buffer(ical_data);
  // Update request
  g_autoptr(SoupSession) session = soup_session_new();
  g_autoptr(SoupMessage) message = soup_message_new("PUT", url);
  g_autoptr(GError) error = NULL;
  if (!message) return FALSE;

  soup_message_headers_replace(soup_message_get_request_headers(message), "Content-Type", "text/calendar; charset=utf-8");
  // Only update if the server version hasn't changed
  soup_message_headers_replace(soup_message_get_request_headers(message), "If-Match", etag);
  soup_message_set_request_body_from_bytes(message, "text/calendar; charset=utf-8", request_body);
  caldav_util_add_basic_auth(message, c);
  g_autoptr(GBytes) response = soup_session_send_and_read(session, message, NULL, &error);
  if (!response) {
    g_warning("CalDAV: Update VTODO request failed: %s", error->message);
    return FALSE;
  }

  guint status = soup_message_get_status(message);
  if (status == SOUP_STATUS_PRECONDITION_FAILED) {
    g_warning("CalDAV: VTODO was modified remotely (ETag mismatch)");
    return FALSE;
  }

  if (status != SOUP_STATUS_OK && status != SOUP_STATUS_NO_CONTENT) {
    g_warning("CalDAV: Update VTODO returned HTTP %u", status);
    return FALSE;
  }

  // Get updated ETag
  const gchar *response_etag = soup_message_headers_get_one(soup_message_get_response_headers(message), "ETag");
  g_autofree gchar *new_etag = g_strdup(response_etag);
  if (!new_etag) new_etag = caldav_util_pull_etag(c, session, url);

  g_object_set(self, "ical", ical, NULL);
  g_object_set(self, "etag", new_etag, NULL);

  return TRUE;
}

void caldav_todo_set_etag(CalDAVTodo *self, const gchar *etag) { g_object_set(self, "etag", etag, NULL); }
void caldav_todo_set_url(CalDAVTodo *self, const gchar *url) { g_object_set(self, "url", url, NULL); }
