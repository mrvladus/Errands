// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "caldav.h"

#define CALDAV_MISS_THRESHOLD 3

// --- DEFINITIONS --- //

struct _CalDAVCalendar {
  CalDAVClient *client;
  gchar *href;
  gchar *uid;
  ESource *registered_source;
  ECalClient *e_cal_client;
  ECalClientView *view;
  GPtrArray *tasks;

  gboolean being_deleted;

  gboolean seen;    // Set during each discovery round
  guint miss_count; // Consecutive polls where the href was absent
};

struct _CalDAVClient {
  gchar *url;
  gchar *username;
  gchar *password;
  void (*cal_added_cb)(CalDAVCalendar *c);
  void (*cal_modified_cb)(CalDAVCalendar *c, const char *name, const char *color);
  void (*cal_removed_cb)(const char *uid);
  void (*task_added_cb)(CalDAVCalendar *c, ICalComponent *task);
  void (*task_modified_cb)(CalDAVCalendar *c, ICalComponent *task);
  void (*task_removed_cb)(CalDAVCalendar *c, const char *uid);

  gchar *caldav_url;
  gchar *principal_url;
  gchar *home_set_url;

  ESourceRegistry *registry;
  ENamedParameters *credentials;
  ESource *server_source;

  GPtrArray *calendars;
};

// --- UTILS --- //

static inline gchar *caldav__compute_source_uid(EWebDAVDiscoveredSource *ds) {
  return g_strdup_printf("caldav-task-list-%s", g_compute_checksum_for_string(G_CHECKSUM_SHA256, ds->href, -1));
}

static inline ENamedParameters *caldav__make_credentials(const char *username, const char *password) {
  ENamedParameters *credentials = e_named_parameters_new();
  e_named_parameters_set(credentials, E_SOURCE_CREDENTIAL_USERNAME, username);
  e_named_parameters_set(credentials, E_SOURCE_CREDENTIAL_PASSWORD, password);
  return credentials;
}

static inline gchar *caldav__uid_from_href(const gchar *href) {
  if (!href || !*href) return NULL;
  g_autofree gchar *s = g_strdup(href);
  gsize len = strlen(s);
  while (len > 0 && s[len - 1] == '/') s[--len] = '\0';
  const gchar *slash = strrchr(s, '/');
  return g_strdup(slash ? slash + 1 : s);
}

static void list_tasks(ECalClient *client) {
  g_autoptr(GError) error = NULL;
  GSList *objects = NULL;
  if (!e_cal_client_get_object_list_sync(client, "#t", &objects, NULL, &error))
    g_error("e_cal_client_get_object_list_sync(): %s", error->message);
  for (GSList *link = objects; link != NULL; link = link->next) {
    ICalComponent *component = link->data;
    printf("\n%s\n", i_cal_component_as_ical_string(component));
  }
  e_client_util_free_object_slist(objects);
}

static gchar *caldav__make_calendar_href(const gchar *calendar_home, const char *uid) {
  g_assert_nonnull(calendar_home);
  g_assert(*calendar_home);

  g_autofree gchar *uuid = g_uuid_string_random();
  g_autofree gchar *segment = g_strdup_printf("%s/", uid ? uid : uuid);
  g_autofree gchar *base = g_str_has_suffix(calendar_home, "/") ? g_strdup(calendar_home) : g_strconcat(calendar_home, "/", NULL);
  g_autoptr(GError) error = NULL;
  gchar *href = g_uri_resolve_relative(base, segment, G_URI_FLAGS_PARSE_RELAXED, &error);
  if (error) g_warning("CalDAV: Failed to construct calendar href from '%s': %s", calendar_home, error->message);

  return href;
}

// --- CALENDAR PRIVATE FUNCTIONS --- //

static inline void on_task_added_cb(ECalClientView *self, const GSList *comps, CalDAVCalendar *c) {
  if (!c->client->task_added_cb) return;
  for (const GSList *l = comps; l != NULL; l = l->next) c->client->task_added_cb(c, l->data);
}

static inline void on_task_modified_cb(ECalClientView *self, const GSList *comps, CalDAVCalendar *c) {
  if (!c->client->task_modified_cb) return;
  for (const GSList *l = comps; l != NULL; l = l->next) c->client->task_modified_cb(c, l->data);
}

static inline void on_task_removed_cb(ECalClientView *self, const GSList *comps, CalDAVCalendar *c) {
  if (!c->client->task_removed_cb) return;
  for (const GSList *l = comps; l != NULL; l = l->next) c->client->task_removed_cb(c, e_cal_component_id_get_uid(l->data));
}

static inline CalDAVCalendar *caldav__connect_discovered_source(CalDAVClient *c, EWebDAVDiscoveredSource *ds) {
  g_assert_nonnull(c);
  g_assert_nonnull(ds);

  g_autoptr(GError) error = NULL;
  g_autofree gchar *uid = caldav__compute_source_uid(ds);
  g_autoptr(ESource) source = e_source_new_with_uid(uid, NULL, &error);
  if (!source) {
    g_warning("CalDAV: Failed to create calendar source '%s': %s", uid, error->message);
    return NULL;
  }
  e_source_set_display_name(source, ds->display_name ? ds->display_name : "CalDAV Task List");
  ESourceTaskList *task_list = e_source_get_extension(source, E_SOURCE_EXTENSION_TASK_LIST);
  e_source_selectable_set_selected(E_SOURCE_SELECTABLE(task_list), TRUE);
  if (ds->color && *ds->color) e_source_selectable_set_color(E_SOURCE_SELECTABLE(task_list), ds->color);
  e_source_selectable_set_order(E_SOURCE_SELECTABLE(task_list), ds->order);
  e_source_backend_set_backend_name(E_SOURCE_BACKEND(task_list), "caldav");
  g_autoptr(GUri) uri = g_uri_parse(ds->href, G_URI_FLAGS_PARSE_RELAXED, &error);
  if (!uri) {
    g_warning("CalDAV: Failed to parse URI '%s': %s", ds->href, error->message);
    return NULL;
  }

  ESourceWebdav *webdav = e_source_get_extension(source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  e_source_webdav_set_uri(webdav, uri);
  if (ds->display_name) e_source_webdav_set_display_name(webdav, ds->display_name);
  e_source_webdav_set_order(webdav, ds->order);
  ESourceAuthentication *auth = e_source_get_extension(source, E_SOURCE_EXTENSION_AUTHENTICATION);
  e_source_authentication_set_method(auth, "Basic");
  e_source_authentication_set_user(auth, c->username);
  if (!e_source_store_password_sync(source, c->password, FALSE, NULL, &error)) {
    g_warning("CalDAV: Failed to store password: %s", error->message);
    return NULL;
  }
  if (!e_source_registry_commit_source_sync(c->registry, source, NULL, &error)) {
    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
      g_warning("CalDAV: Failed to commit source: %s", error->message);
      return NULL;
    }
    g_clear_error(&error);
  }

  ESource *registered_source = e_source_registry_ref_source(c->registry, uid);
  if (!registered_source) {
    g_warning("CalDAV: Failed to ref source: source '%s' not found", uid);
    return NULL;
  }

  EClient *e_client = e_cal_client_connect_sync(registered_source, E_CAL_CLIENT_SOURCE_TYPE_TASKS, 1, NULL, &error);
  if (!e_client) {
    g_warning("CalDAV: Failed to connect calendar '%s': %s", uid, error->message);
    g_object_unref(registered_source);
    return NULL;
  }

  if (!e_client_refresh_sync(e_client, NULL, &error)) {
    g_warning("CalDAV: Refresh failed for '%s': %s", ds->href, error->message);
    g_clear_error(&error);
  }

  ECalClientView *view = NULL;
  if (!e_cal_client_get_view_sync(E_CAL_CLIENT(e_client), "#t", &view, NULL, &error)) {
    g_warning("CalDAV: Failed to get client view: %s", error->message);
    g_object_unref(e_client);
    g_object_unref(registered_source);
    return NULL;
  }

  CalDAVCalendar *calendar = g_new0(CalDAVCalendar, 1);
  calendar->client = c;
  calendar->href = g_strdup(ds->href);
  calendar->uid = caldav__uid_from_href(ds->href);
  calendar->registered_source = registered_source;
  calendar->e_cal_client = E_CAL_CLIENT(e_client);
  calendar->view = view;

  g_signal_connect(view, "objects-added", G_CALLBACK(on_task_added_cb), calendar);
  g_signal_connect(view, "objects-modified", G_CALLBACK(on_task_modified_cb), calendar);
  g_signal_connect(view, "objects-removed", G_CALLBACK(on_task_removed_cb), calendar);
  e_cal_client_view_start(view, &error);
  if (error) {
    g_warning("CalDAV: Failed to start client view: %s", error->message);
    caldav_calendar_free(calendar);
    return NULL;
  }

  return calendar;
}

static inline void caldav__calendar_poll(CalDAVCalendar *calendar) {
  g_message("CalDAV: Poll Calendar: %s", calendar->href);

  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(calendar->registered_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), calendar->client->credentials);
  GSList *resources = NULL;
  gboolean success = e_webdav_session_list_sync(session, NULL, E_WEBDAV_DEPTH_THIS,
                                                E_WEBDAV_LIST_DISPLAY_NAME | E_WEBDAV_LIST_COLOR, &resources, NULL, &error);
  if (!success) {
    g_warning("CalDAV: Failed to query calendar properties: %s", error->message);
    return;
  }

  EWebDAVResource *resource = resources ? resources->data : NULL;
  if (resource) {
    ESourceTaskList *task_list = e_source_get_extension(calendar->registered_source, E_SOURCE_EXTENSION_TASK_LIST);
    gboolean changed = FALSE;
    const char *local_name = e_source_get_display_name(calendar->registered_source);
    if (resource->display_name && g_strcmp0(local_name, resource->display_name) != 0) {
      e_source_set_display_name(calendar->registered_source, resource->display_name);
      changed = TRUE;
    }
    const gchar *local_color = e_source_selectable_get_color(E_SOURCE_SELECTABLE(task_list));
    if (resource->color && g_strcmp0(local_color, resource->color) != 0) {
      e_source_selectable_set_color(E_SOURCE_SELECTABLE(task_list), resource->color);
      changed = TRUE;
    }
    if (changed) {
      if (!e_source_registry_commit_source_sync(calendar->client->registry, calendar->registered_source, NULL, &error))
        g_warning("CalDAV: Failed to commit source: %s", error->message);
      else {
        if (calendar->client->cal_modified_cb)
          calendar->client->cal_modified_cb(calendar, resource->display_name, resource->color);
      }
    }
  }

  if (!e_client_refresh_sync(E_CLIENT(calendar->e_cal_client), NULL, &error))
    g_warning("CalDAV: CalDAV refresh failed: %s", error->message);

  g_slist_free_full(resources, (GDestroyNotify)e_webdav_resource_free);
}

// --- CALENDAR PUBLIC FUNCTIONS --- //

const gchar *caldav_calendar_get_color(const CalDAVCalendar *calendar) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->registered_source);

  ESourceTaskList *task_list = e_source_get_extension(calendar->registered_source, E_SOURCE_EXTENSION_TASK_LIST);
  return e_source_selectable_get_color(E_SOURCE_SELECTABLE(task_list));
}

const gchar *caldav_calendar_get_name(const CalDAVCalendar *calendar) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->registered_source);

  return e_source_get_display_name(calendar->registered_source);
}

const gchar *caldav_calendar_get_href(const CalDAVCalendar *calendar) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->registered_source);

  return calendar->href;
}

const gchar *caldav_calendar_get_uid(const CalDAVCalendar *calendar) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->registered_source);

  return calendar->uid;
}

gboolean caldav_calendar_set_properties(const CalDAVCalendar *calendar, const gchar *name, const gchar *color) {
  g_assert_nonnull(calendar);

  GSList *changes = NULL;
  if (name) changes = g_slist_append(changes, e_webdav_property_change_new_set(E_WEBDAV_NS_DAV, "displayname", name));
  if (color) changes = g_slist_append(changes, e_webdav_property_change_new_set(E_WEBDAV_NS_ICAL, "calendar-color", color));
  if (!changes) return FALSE;

  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(calendar->registered_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), calendar->client->credentials);
  gboolean success = e_webdav_session_update_properties_sync(session, NULL, changes, NULL, &error);
  if (!success || error) {
    g_warning("CalDAV: %s", error->message);
    return FALSE;
  }

  g_slist_free_full(changes, (GDestroyNotify)e_webdav_property_change_free);

  ESourceTaskList *task_list = e_source_get_extension(calendar->registered_source, E_SOURCE_EXTENSION_TASK_LIST);
  if (name) e_source_set_display_name(calendar->registered_source, name);
  if (color) e_source_selectable_set_color(E_SOURCE_SELECTABLE(task_list), color);
  e_source_registry_commit_source_sync(calendar->client->registry, calendar->registered_source, NULL, &error);
  if (error) {
    g_warning("CalDAV: %s", error->message);
    return FALSE;
  }

  return TRUE;
}

gboolean caldav_calendar_set_color(const CalDAVCalendar *calendar, const gchar *color) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(color);

  return caldav_calendar_set_properties(calendar, NULL, color);
}

gboolean caldav_calendar_set_name(const CalDAVCalendar *calendar, const gchar *name) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(name);

  return caldav_calendar_set_properties(calendar, name, NULL);
}

gboolean caldav_calendar_push_task(CalDAVCalendar *calendar, ICalComponent *task) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->e_cal_client);
  g_assert_nonnull(task);
  g_assert(I_CAL_IS_COMPONENT(task));
  g_assert(i_cal_component_isa(task) == I_CAL_VTODO_COMPONENT);

  // The backend may assign a UID if the component lacks one.
  if (!i_cal_component_get_uid(task)) {
    g_autofree gchar *new_uid = g_uuid_string_random();
    i_cal_component_set_uid(task, new_uid);
  }
  // Some backends require DTSTAMP.
  if (!i_cal_component_get_dtstamp(task)) {
    g_autoptr(ICalTime) now = i_cal_time_new_current_with_zone(i_cal_timezone_get_utc_timezone());
    i_cal_component_set_dtstamp(task, now);
  }
  const gchar *uid = i_cal_component_get_uid(task);
  g_autoptr(GError) error = NULL;
  if (!e_cal_client_modify_object_sync(calendar->e_cal_client, task, E_CAL_OBJ_MOD_THIS, E_CAL_OPERATION_FLAG_NONE, NULL,
                                       &error)) {
    g_warning("CalDAV: Failed to update task '%s': %s", uid, error->message);
    g_message("CalDAV: Trying to create task: '%s'", uid);
    g_clear_error(&error);
    g_autofree gchar *out_uid = NULL;
    if (!e_cal_client_create_object_sync(calendar->e_cal_client, task, E_CAL_OPERATION_FLAG_CONFLICT_KEEP_LOCAL, &out_uid, NULL,
                                         &error)) {
      g_warning("CalDAV: Failed to create task: %s", error->message);
      return FALSE;
    } else {
      if (uid && out_uid && g_strcmp0(uid, out_uid) != 0) {
        g_message("CalDAV: UID changed by server: '%s' -> '%s'", uid, out_uid);
        i_cal_component_set_uid(task, out_uid);
      }
      return TRUE;
    }
    return FALSE;
  }
  g_message("CalDAV: Updated task: '%s'", uid);
  return TRUE;
}

gboolean caldav_calendar_remove_task(CalDAVCalendar *calendar, const char *uid) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->e_cal_client);
  g_assert_nonnull(uid);

  g_autoptr(GError) error = NULL;
  if (!e_cal_client_remove_object_sync(calendar->e_cal_client, uid, NULL, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, NULL,
                                       &error)) {
    g_warning("CalDAV: Failed to remove task '%s': %s", uid, error->message);
    return FALSE;
  }
  g_message("CalDAV: Removed task: '%s'", uid);
  return TRUE;
}

gboolean caldav_calendar_remove(const CalDAVCalendar *calendar) {
  g_assert_nonnull(calendar);
  g_assert_nonnull(calendar->client);
  g_assert_nonnull(calendar->href);
  g_assert_nonnull(calendar->registered_source);

  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(calendar->registered_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), calendar->client->credentials);
  if (!e_webdav_session_delete_sync(session, calendar->href, E_WEBDAV_DEPTH_INFINITY, NULL, NULL, &error)) {
    g_warning("CalDAV: Failed to remove calendar '%s': %s", calendar->href, error ? error->message : "unknown error");
    return FALSE;
  }
  e_cal_client_view_stop(calendar->view, NULL);
  if (!e_source_remove_sync(calendar->registered_source, NULL, &error)) {
    // The server deletion already succeeded. Keep the calendar in the array so the caller does not get a dangling object; the
    // next discovery round can clean up the local source.
    g_warning("CalDAV: Remote calendar '%s' was deleted, but local source removal failed: %s", calendar->href, error->message);
    return FALSE;
  }
  g_ptr_array_remove(calendar->client->calendars, (CalDAVCalendar *)calendar);
  calendar = NULL;

  return TRUE;
}

void caldav_calendar_free(CalDAVCalendar *c) {
  g_clear_object(&c->e_cal_client);
  g_clear_object(&c->registered_source);
  g_clear_object(&c->view);
  g_clear_pointer(&c->href, g_free);
  g_clear_pointer(&c->uid, g_free);
  g_clear_pointer(&c->tasks, g_ptr_array_unref);
  g_clear_pointer(&c, g_free);
}

// ---------- CLIENT PRIVATE FUNCTIONS ---------- //

typedef struct {
  gchar *href;
} _CalDAVDiscoverQuery;

static inline gchar *caldav__discover_caldav_url(CalDAVClient *c) {
  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(c->server_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), c->credentials);

  g_autoptr(GUri) base = g_uri_parse(c->url, G_URI_FLAGS_PARSE_RELAXED, &error);
  g_autoptr(GUri) well_known_uri = g_uri_build(G_URI_FLAGS_NONE, g_uri_get_scheme(base), NULL, g_uri_get_host(base),
                                               g_uri_get_port(base), "/.well-known/caldav", NULL, NULL);
  g_autofree gchar *well_known = g_uri_to_string(well_known_uri);
  if (!well_known) return NULL;
  g_autoptr(SoupMessage) message = e_webdav_session_new_message(session, SOUP_METHOD_GET, well_known, &error);
  if (!message) return NULL;
  GByteArray *bytes = e_soup_session_send_message_simple_sync(E_SOUP_SESSION(session), message, NULL, &error);
  if (!bytes) return NULL;
  g_byte_array_free(bytes, TRUE);
  GUri *final_uri = soup_message_get_uri(message);
  gchar *result = NULL;
  if (final_uri) result = g_uri_to_string(final_uri);
  return result;
}

static inline gboolean caldav__principal_cb(EWebDAVSession *webdav, xmlNodePtr prop_node, const GUri *request_uri,
                                            const gchar *href, guint status_code, gpointer user_data) {
  if (status_code != SOUP_STATUS_OK) return TRUE;
  _CalDAVDiscoverQuery *query = user_data;
  xmlNodePtr node;
  const xmlChar *value;
  // Prefer DAV:current-user-principal
  node = e_xml_find_in_hierarchy(prop_node, E_WEBDAV_NS_DAV, "current-user-principal", E_WEBDAV_NS_DAV, "href", NULL, NULL);
  value = e_xml_get_node_text(node);
  if (value && *value) {
    g_free(query->href);
    query->href = e_webdav_session_ensure_full_uri(webdav, request_uri, (const gchar *)value);
    return TRUE;
  }
  // Fallback to DAV:principal-URL
  node = e_xml_find_in_hierarchy(prop_node, E_WEBDAV_NS_DAV, "principal-URL", E_WEBDAV_NS_DAV, "href", NULL, NULL);
  value = e_xml_get_node_text(node);
  if (value && *value) {
    g_free(query->href);
    query->href = e_webdav_session_ensure_full_uri(webdav, request_uri, (const gchar *)value);
  }
  return TRUE;
}

static inline gchar *caldav__discover_principal_url(CalDAVClient *c) {
  if (!c->caldav_url) return NULL;

  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(c->server_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), c->credentials);

  _CalDAVDiscoverQuery query = {0};
  g_autoptr(EXmlDocument) xml = e_xml_document_new(E_WEBDAV_NS_DAV, "propfind");
  e_xml_document_start_element(xml, E_WEBDAV_NS_DAV, "prop");
  e_xml_document_add_empty_element(xml, E_WEBDAV_NS_DAV, "current-user-principal");
  e_xml_document_add_empty_element(xml, E_WEBDAV_NS_DAV, "principal-URL");
  e_xml_document_end_element(xml);
  gboolean success = e_webdav_session_propfind_sync(session, c->caldav_url, E_WEBDAV_DEPTH_THIS, xml, caldav__principal_cb,
                                                    &query, NULL, &error);
  if (!success) {
    g_free(query.href);
    return NULL;
  }
  if (!query.href || !*query.href) {
    // g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Server did not return current-user-principal or
    // principal-URL");
    return NULL;
  }
  return query.href;
}

static inline gboolean caldav__discover_home_set_cb(EWebDAVSession *webdav, xmlNodePtr prop_node, const GUri *request_uri,
                                                    const gchar *href, guint status_code, gpointer user_data) {
  if (status_code != SOUP_STATUS_OK) return TRUE;

  _CalDAVDiscoverQuery *query = user_data;
  const xmlChar *value;
  xmlNodePtr set_node = e_xml_find_child(prop_node, E_WEBDAV_NS_CALDAV, "calendar-home-set");
  for (xmlNodePtr node = e_xml_find_child(set_node, E_WEBDAV_NS_DAV, "href"); node;
       node = e_xml_find_next_sibling(node, E_WEBDAV_NS_DAV, "href")) {
    value = e_xml_get_node_text(node);
    if (value && *value) {
      g_free(query->href);
      query->href = e_webdav_session_ensure_full_uri(webdav, request_uri, (const gchar *)value);
      return TRUE;
    }
  }
  return TRUE;
}

static inline gchar *caldav__discover_home_set(CalDAVClient *c) {
  if (!c->principal_url) return NULL;

  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(c->server_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), c->credentials);

  _CalDAVDiscoverQuery query = {0};
  g_autoptr(EXmlDocument) xml = e_xml_document_new(E_WEBDAV_NS_DAV, "propfind");
  e_xml_document_start_element(xml, E_WEBDAV_NS_DAV, "prop");
  e_xml_document_add_empty_element(xml, E_WEBDAV_NS_CALDAV, "calendar-home-set");
  e_xml_document_end_element(xml);
  gboolean success = e_webdav_session_propfind_sync(session, c->principal_url, E_WEBDAV_DEPTH_THIS, xml,
                                                    caldav__discover_home_set_cb, &query, NULL, &error);
  if (!success) {
    g_free(query.href);
    return NULL;
  }
  if (!query.href || !*query.href) {
    g_free(query.href);
    // g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Server did not return calendar-home-set");
    return NULL;
  }
  return query.href;
}

// ---------- CLIENT PUBLIC FUNCTIONS ---------- //

CalDAVClient *caldav_client_new(CalDAVClientConfig config) {
  g_assert_nonnull(config.url);
  g_assert_nonnull(config.username);
  g_assert_nonnull(config.password);

  g_autoptr(GError) error = NULL;

  ESourceRegistry *registry = e_source_registry_new_sync(NULL, &error);
  if (error) {
    g_warning("CalDAV: Failed to create source registry: %s\n", error->message);
    return NULL;
  }

  ESource *server_source = e_source_new(NULL, NULL, &error);
  if (error) {
    g_warning("CalDAV: Failed to create server source: %s\n", error->message);
    return NULL;
  }

  e_source_set_display_name(server_source, "CalDAV");
  g_autoptr(GUri) uri = g_uri_parse(config.url, G_URI_FLAGS_PARSE_RELAXED, &error);
  if (error) {
    g_warning("CalDAV: Failed to parse URI: %s\n", error->message);
    return NULL;
  }

  ESourceWebdav *webdav = e_source_get_extension(server_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
  e_source_webdav_set_uri(webdav, uri);
  ESourceAuthentication *auth = e_source_get_extension(server_source, E_SOURCE_EXTENSION_AUTHENTICATION);
  e_source_authentication_set_method(auth, "Basic");
  e_source_authentication_set_user(auth, config.username);
  if (!e_source_store_password_sync(server_source, config.password, FALSE, NULL, &error)) {
    g_warning("CalDAV: Failed to store password: %s\n", error->message);
    return NULL;
  }

  CalDAVClient *c = g_new0(CalDAVClient, 1);
  c->url = g_strdup(config.url);
  c->username = g_strdup(config.username);
  c->password = g_strdup(config.password);
  c->credentials = caldav__make_credentials(c->username, c->password);
  c->registry = registry;
  c->server_source = server_source;
  c->calendars = g_ptr_array_new_with_free_func((GDestroyNotify)caldav_calendar_free);
  c->cal_added_cb = config.cal_added_cb;
  c->cal_modified_cb = config.cal_modified_cb;
  c->cal_removed_cb = config.cal_removed_cb;
  c->task_added_cb = config.task_added_cb;
  c->task_modified_cb = config.task_modified_cb;
  c->task_removed_cb = config.task_removed_cb;

  c->caldav_url = caldav__discover_caldav_url(c);
  c->principal_url = caldav__discover_principal_url(c);
  c->home_set_url = caldav__discover_home_set(c);

  if (!c->caldav_url) c->caldav_url = g_strdup(config.url);
  if (!c->principal_url) c->principal_url = g_strdup(config.url);
  if (!c->home_set_url) c->home_set_url = g_strdup(config.url);

  if (!caldav_client_poll(c)) return NULL;

  return c;
}

gboolean caldav_client_poll(CalDAVClient *c) {
  g_assert_nonnull(c);
  g_assert_nonnull(c->server_source);
  g_assert_nonnull(c->credentials);
  g_assert_nonnull(c->calendars);

  g_message("CalDAV: Poll Client: %s", c->url);

  g_autoptr(GError) error = NULL;

  GSList *discovered = NULL;
  if (!e_webdav_discover_sources_sync(c->server_source, NULL, E_WEBDAV_DISCOVER_SUPPORTS_TASKS, c->credentials, NULL, NULL,
                                      &discovered, NULL, NULL, &error)) {
    g_warning("CalDAV: Discovery failed: %s", error->message);
    return FALSE;
  }

  // Mark all calendars as not seen for this round
  for (guint i = 0; i < c->calendars->len; i++) {
    CalDAVCalendar *cal = g_ptr_array_index(c->calendars, i);
    cal->seen = FALSE;
  }

  // Walk discovery results: refresh known calendars, connect new ones
  for (GSList *l = discovered; l != NULL; l = l->next) {
    EWebDAVDiscoveredSource *ds = l->data;
    if (!(ds->supports & E_WEBDAV_DISCOVER_SUPPORTS_TASKS)) continue;
    g_autofree gchar *uid = caldav__compute_source_uid(ds);
    CalDAVCalendar *existing = NULL;
    for (guint i = 0; i < c->calendars->len; i++) {
      CalDAVCalendar *cal = g_ptr_array_index(c->calendars, i);
      if (g_strcmp0(e_source_get_uid(cal->registered_source), uid) == 0) {
        existing = cal;
        break;
      }
    }
    if (existing) {
      existing->seen = TRUE;
      existing->miss_count = 0;
      caldav__calendar_poll(existing); // Refresh calendar
      continue;
    }
    // Not tracked yet → brand new calendar on the server
    CalDAVCalendar *cal = caldav__connect_discovered_source(c, ds);
    if (cal) {
      cal->seen = TRUE;
      cal->miss_count = 0;
      g_ptr_array_add(c->calendars, cal);
      if (c->cal_added_cb) c->cal_added_cb(cal);
    }
  }

  // Remove calendars that weren't seen this round. Iterate backwards because we mutate the array.
  for (gint i = (gint)c->calendars->len - 1; i >= 0; i--) {
    CalDAVCalendar *cal = g_ptr_array_index(c->calendars, i);
    if (cal->seen) continue;
    cal->miss_count++;
    g_message("CalDAV: Calendar '%s' not seen (%u/%u)", cal->href, cal->miss_count, CALDAV_MISS_THRESHOLD);
    if (cal->miss_count < CALDAV_MISS_THRESHOLD) continue;
    if (c->cal_removed_cb) c->cal_removed_cb(cal->uid); // uid still valid here
    // Delete the local ESource
    e_cal_client_view_stop(cal->view, NULL);
    if (!e_source_remove_sync(cal->registered_source, NULL, &error)) {
      g_warning("CalDAV: Failed to remove source '%s': %s", cal->uid, error->message);
      g_clear_error(&error);
    }
    g_ptr_array_remove_index(c->calendars, i); // caldav_calendar_free is called
  }

  g_clear_pointer(&discovered, e_webdav_discover_free_discovered_sources);
  return TRUE;
}

CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *c, const char *uid, const gchar *name, const gchar *color) {
  g_assert_nonnull(c);
  g_assert_nonnull(c->home_set_url);
  g_assert_nonnull(name);
  g_assert_nonnull(color);
  g_assert(*name);
  g_assert(*color && color[0] == '#');

  g_autoptr(GError) error = NULL;
  g_autoptr(EWebDAVSession) session = e_webdav_session_new(c->server_source);
  e_soup_session_set_credentials(E_SOUP_SESSION(session), c->credentials);
  g_autofree gchar *href = caldav__make_calendar_href(c->home_set_url, uid);

  if (!e_webdav_session_mkcalendar_sync(session, href, name, NULL, color, E_WEBDAV_RESOURCE_SUPPORTS_TASKS, NULL, &error)) {
    g_warning("CalDAV: Failed to create calendar: %s", error->message);
    return FALSE;
  }

  EWebDAVDiscoveredSource discovered = {0};
  discovered.href = href;
  discovered.display_name = (gchar *)name;
  discovered.color = (gchar *)color;
  discovered.supports = E_WEBDAV_DISCOVER_SUPPORTS_TASKS;
  discovered.order = 0;

  CalDAVCalendar *calendar = caldav__connect_discovered_source(c, &discovered);
  g_ptr_array_add(c->calendars, calendar);
  if (c->cal_added_cb) c->cal_added_cb(calendar);

  return calendar;

  // caldav_client_poll(c);

  //   if (!href) return FALSE;
  //   g_autoptr(EWebDAVSession) session = e_webdav_session_new(c->server_source);
  //   e_soup_session_set_credentials(E_SOUP_SESSION(session), c->credentials);
  //
  //   g_autoptr(EXmlDocument) xml = e_xml_document_new(E_WEBDAV_NS_DAV, "mkcol");
  //   e_xml_document_add_namespaces(xml, "C", E_WEBDAV_NS_CALDAV, "I", E_WEBDAV_NS_ICAL, NULL);
  //   e_xml_document_start_element(xml, E_WEBDAV_NS_DAV, "set");
  //   e_xml_document_start_element(xml, E_WEBDAV_NS_DAV, "prop");
  //   e_xml_document_start_element(xml, E_WEBDAV_NS_DAV, "resourcetype");
  //   e_xml_document_add_empty_element(xml, E_WEBDAV_NS_DAV, "collection");
  //   e_xml_document_add_empty_element(xml, E_WEBDAV_NS_CALDAV, "calendar");
  //   e_xml_document_end_element(xml);
  //   e_xml_document_start_element(xml, E_WEBDAV_NS_CALDAV, "supported-calendar-component-set");
  //   e_xml_document_start_element(xml, E_WEBDAV_NS_CALDAV, "comp");
  //   e_xml_document_add_attribute(xml, NULL, "name", "VTODO");
  //   e_xml_document_end_element(xml);
  //   e_xml_document_end_element(xml);
  //   e_xml_document_start_text_element(xml, E_WEBDAV_NS_DAV, "displayname");
  //   e_xml_document_write_string(xml, name);
  //   e_xml_document_end_element(xml);
  //   e_xml_document_start_text_element(xml, E_WEBDAV_NS_ICAL, "calendar-color");
  //   e_xml_document_write_string(xml, color);
  //   e_xml_document_end_element(xml);
  //   e_xml_document_end_element(xml);
  //   e_xml_document_end_element(xml);
  //
  //   g_autoptr(SoupMessage) message = e_webdav_session_new_message(session, SOUP_METHOD_MKCOL, href, &error);
  //   if (!message) return FALSE;
  //
  //   gsize content_length = 0;
  //   g_autofree gchar *content = e_xml_document_get_content(xml, &content_length);
  //   if (!content) {
  //     g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Failed to serialize MKCOL request");
  //     return FALSE;
  //   }
  //
  //   e_soup_session_util_set_message_request_body_from_data(message, FALSE, E_WEBDAV_CONTENT_TYPE_XML, content, content_length,
  //                                                          g_free);
  //
  //   g_autoptr(GByteArray) response = e_soup_session_send_message_simple_sync(E_SOUP_SESSION(session), message, NULL, &error);
  //   gboolean success = response != NULL && SOUP_STATUS_IS_SUCCESSFUL(soup_message_get_status(message));
  //   if (!success) {
  //     if (!error) {
  //       g_set_error(&error, E_SOUP_SESSION_ERROR, soup_message_get_status(message), "Failed to create calendar: HTTP %u (%s)",
  //                   soup_message_get_status(message), soup_message_get_reason_phrase(message));
  //     }
  //     g_warning("CalDAV: Failed to create calendar '%s': %s", href, error->message);
  //     return FALSE;
  //   }
  //   g_message("CalDAV: Created remote calendar: %s", href);
  //
  //   EWebDAVDiscoveredSource discovered = {0};
  //   discovered.href = href;
  //   discovered.display_name = (gchar *)name;
  //   discovered.color = (gchar *)color;
  //   discovered.supports = E_WEBDAV_DISCOVER_SUPPORTS_TASKS;
  //   discovered.order = 0;
  //
  //   CalDAVCalendar *calendar = caldav__connect_discovered_source(c, &discovered);
  //   if (!calendar) {
  //     g_clear_error(&error);
  //     if (!e_webdav_session_delete_sync(session, href, E_WEBDAV_DEPTH_INFINITY, NULL, NULL, &error))
  //       g_warning("CalDAV: Failed to roll back calendar '%s': %s", href, error->message);
  //     return FALSE;
  //   }
  //   g_ptr_array_add(c->calendars, calendar);
  //   calendar->seen = TRUE;
  //   calendar->miss_count = 0;
  //   if (c->cal_added_cb) c->cal_added_cb(calendar);
  //   if (out_calendar) *out_calendar = calendar;

  // return TRUE;
}

CalDAVCalendar *caldav_client_find_calendar(CalDAVClient *c, const gchar *uid) {
  g_assert_nonnull(c);
  g_assert_nonnull(uid);

  for (guint i = 0; i < c->calendars->len; ++i) {
    CalDAVCalendar *cal = g_ptr_array_index(c->calendars, i);
    if (g_str_equal(cal->uid, uid)) return cal;
  }
  return NULL;
}

void caldav_client_free(CalDAVClient *c) {
  if (!c) return;
  g_clear_object(&c->server_source);
  g_clear_object(&c->registry);
  g_clear_pointer(&c->calendars, g_ptr_array_unref);
  g_clear_pointer(&c->credentials, e_named_parameters_free);
  g_clear_pointer(&c->url, g_free);
  g_clear_pointer(&c->username, g_free);
  g_clear_pointer(&c->password, g_free);
  g_clear_pointer(&c->caldav_url, g_free);
  g_clear_pointer(&c->principal_url, g_free);
  g_clear_pointer(&c->home_set_url, g_free);
  g_clear_pointer(&c, g_free);
}
