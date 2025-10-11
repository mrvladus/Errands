#include <gtk/gtk.h>

#define ROW_HEIGHT     32
#define TASK_POOL_SIZE 40

/* ------------------------------
 * Data Structures
 * ------------------------------ */
typedef struct _TaskData TaskData;
struct _TaskData {
  char *title;
  gboolean completed;
  int due_date;
  GPtrArray *children;
  gboolean expanded;
};

typedef struct {
  TaskData *data;
  int depth;
} VisibleRow;

typedef struct {
  GtkWidget *box;
  GtkWidget *expander;
  GtkWidget *label;
  int row_index;
} TaskWidget;

/* ------------------------------
 * Globals
 * ------------------------------ */
static GPtrArray *all_tasks = NULL;
static GPtrArray *visible_rows = NULL;
static TaskWidget task_pool[TASK_POOL_SIZE];
static GtkWidget *top_spacer = NULL;
static GtkWidget *bottom_spacer = NULL;
static GtkAdjustment *vadj = NULL;
static gboolean current_hide_completed = FALSE;
static gboolean current_sort_due = FALSE;

/* ------------------------------
 * Sorting and Filtering
 * ------------------------------ */
static gint sort_by_title(gconstpointer a, gconstpointer b) {
  TaskData *ta = ((VisibleRow *)a)->data;
  TaskData *tb = ((VisibleRow *)b)->data;
  return g_strcmp0(ta->title, tb->title);
}

static gint sort_by_due_date(gconstpointer a, gconstpointer b) {
  TaskData *ta = ((VisibleRow *)a)->data;
  TaskData *tb = ((VisibleRow *)b)->data;
  return ta->due_date - tb->due_date;
}

/* ------------------------------
 * Flatten the Tree
 * ------------------------------ */
static void flatten_task(TaskData *td, int depth, gboolean hide_completed, GPtrArray *out) {
  if (hide_completed && td->completed) return;

  VisibleRow *row = g_new0(VisibleRow, 1);
  row->data = td;
  row->depth = depth;
  g_ptr_array_add(out, row);

  if (td->expanded && td->children) {
    for (guint i = 0; i < td->children->len; i++) {
      TaskData *child = g_ptr_array_index(td->children, i);
      flatten_task(child, depth + 1, hide_completed, out);
    }
  }
}

static void rebuild_visible_rows(gboolean hide_completed, gboolean sort_due) {
  if (visible_rows) g_ptr_array_free(visible_rows, TRUE);
  visible_rows = g_ptr_array_new_with_free_func(g_free);

  for (guint i = 0; i < all_tasks->len; i++) {
    TaskData *td = g_ptr_array_index(all_tasks, i);
    flatten_task(td, 0, hide_completed, visible_rows);
  }

  if (sort_due) g_ptr_array_sort(visible_rows, sort_by_due_date);
  else g_ptr_array_sort(visible_rows, sort_by_title);
}

/* ------------------------------
 * Binding Widgets
 * ------------------------------ */
static void on_expander_clicked(GtkButton *btn, gpointer user_data);

static void bind_task_widget(TaskWidget *tw, VisibleRow *row) {
  TaskData *td = row->data;

  char buf[256];
  snprintf(buf, sizeof(buf), "%s %s (due %d)", td->title, td->completed ? "[done]" : "", td->due_date);
  gtk_label_set_text(GTK_LABEL(tw->label), buf);

  gtk_widget_set_margin_start(tw->box, row->depth * 20);

  if (td->children && td->children->len > 0) {
    gtk_widget_show(tw->expander);
    gtk_button_set_label(GTK_BUTTON(tw->expander), td->expanded ? "▼" : "▶");
    g_signal_handlers_disconnect_by_func(tw->expander, on_expander_clicked, NULL);
    g_signal_connect(tw->expander, "clicked", G_CALLBACK(on_expander_clicked), td);
  } else {
    gtk_widget_hide(tw->expander);
  }
}

/* ------------------------------
 * Recycler
 * ------------------------------ */
static void update_visible_widgets(void) {
  int first_row = (int)(gtk_adjustment_get_value(vadj) / ROW_HEIGHT);

  if (first_row < 0) first_row = 0;
  if (first_row > (int)visible_rows->len - TASK_POOL_SIZE) first_row = (int)visible_rows->len - TASK_POOL_SIZE;
  if (first_row < 0) first_row = 0;

  for (int i = 0; i < TASK_POOL_SIZE; i++) {
    int row_index = first_row + i;
    if (row_index >= (int)visible_rows->len) {
      gtk_widget_hide(task_pool[i].box);
      continue;
    }
    gtk_widget_show(task_pool[i].box);

    VisibleRow *row = g_ptr_array_index(visible_rows, row_index);
    bind_task_widget(&task_pool[i], row);
  }

  int top_h = first_row * ROW_HEIGHT;
  int bottom_h = (visible_rows->len - (first_row + TASK_POOL_SIZE)) * ROW_HEIGHT;
  if (bottom_h < 0) bottom_h = 0;

  gtk_widget_set_size_request(top_spacer, -1, top_h);
  gtk_widget_set_size_request(bottom_spacer, -1, bottom_h);
}

/* ------------------------------
 * Callbacks
 * ------------------------------ */
static void on_adjustment_changed(GtkAdjustment *adj, gpointer user_data) { update_visible_widgets(); }

static void on_toggle_filter(GtkToggleButton *btn, gpointer user_data) {
  current_hide_completed = gtk_toggle_button_get_active(btn);
  rebuild_visible_rows(current_hide_completed, current_sort_due);
  update_visible_widgets();
}

static void on_toggle_sort(GtkToggleButton *btn, gpointer user_data) {
  current_sort_due = gtk_toggle_button_get_active(btn);
  rebuild_visible_rows(current_hide_completed, current_sort_due);
  update_visible_widgets();
}

static void on_expander_clicked(GtkButton *btn, gpointer user_data) {
  TaskData *td = user_data;
  td->expanded = !td->expanded;
  rebuild_visible_rows(current_hide_completed, current_sort_due);
  update_visible_widgets();
}

/* ------------------------------
 * Test Data
 * ------------------------------ */
static TaskData *make_task(const char *title, gboolean completed, int due) {
  TaskData *td = g_new0(TaskData, 1);
  td->title = g_strdup(title);
  td->completed = completed;
  td->due_date = due;
  td->children = g_ptr_array_new();
  td->expanded = FALSE;
  return td;
}

static void build_test_data(void) {
  all_tasks = g_ptr_array_new();

  for (int i = 0; i < 30000; i++) {
    TaskData *parent = make_task(g_strdup_printf("Parent %d", i), (i % 5 == 0), 100 - i);

    for (int j = 0; j < 5; j++) {
      TaskData *child = make_task(g_strdup_printf("Child %d.%d", i, j), (j % 2 == 0), 50 - j);
      g_ptr_array_add(parent->children, child);

      for (int k = 0; k < 2; k++) {
        TaskData *grand = make_task(g_strdup_printf("Grandchild %d.%d.%d", i, j, k), FALSE, 20 - k);
        g_ptr_array_add(child->children, grand);
      }
    }
    g_ptr_array_add(all_tasks, parent);
  }
}

/* ------------------------------
 * Application Activate
 * ------------------------------ */
static void on_activate(GtkApplication *app, gpointer user_data) {
  build_test_data();

  GtkWidget *win = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(win), 500, 600);
  gtk_window_set_title(GTK_WINDOW(win), "Task Recycler Demo");

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_window_set_child(GTK_WINDOW(win), vbox);

  GtkWidget *filter_btn = gtk_check_button_new_with_label("Hide completed");
  GtkWidget *sort_btn = gtk_check_button_new_with_label("Sort by due date");
  gtk_box_append(GTK_BOX(vbox), filter_btn);
  gtk_box_append(GTK_BOX(vbox), sort_btn);

  GtkWidget *scroller = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scroller, TRUE);
  gtk_box_append(GTK_BOX(vbox), scroller);

  GtkWidget *list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), list_box);

  top_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(list_box), top_spacer);

  for (int i = 0; i < TASK_POOL_SIZE; i++) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_size_request(hbox, -1, ROW_HEIGHT);

    GtkWidget *exp = gtk_button_new_with_label("▶");
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(hbox), exp);
    gtk_box_append(GTK_BOX(hbox), lbl);

    gtk_box_append(GTK_BOX(list_box), hbox);

    task_pool[i].box = hbox;
    task_pool[i].expander = exp;
    task_pool[i].label = lbl;
    task_pool[i].row_index = -1;
  }

  bottom_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(list_box), bottom_spacer);

  vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller));
  g_signal_connect(vadj, "value-changed", G_CALLBACK(on_adjustment_changed), NULL);
  g_signal_connect(filter_btn, "toggled", G_CALLBACK(on_toggle_filter), NULL);
  g_signal_connect(sort_btn, "toggled", G_CALLBACK(on_toggle_sort), NULL);

  rebuild_visible_rows(FALSE, FALSE);
  update_visible_widgets();

  gtk_window_present(GTK_WINDOW(win));
}

/* ------------------------------
 * Main
 * ------------------------------ */
int main(int argc, char *argv[]) {
  GtkApplication *app = gtk_application_new("com.example.taskrecycler", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
