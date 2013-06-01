#include <assert.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include "events_db.h"
#include "gui.h"

#define MARKER_MAXIMUM_SIZE 1024

enum {
	MARKERS_CHECK,
	MARKERS_NAME,
	MARKERS_TOTAL
} TypesFields;

struct Session{
	struct EventsDb *eventsdb;
	GtkWidget *markers_tree_view;
	GtkWidget *events_tree_view;
	GtkWidget *events_tree_view_parent;
};

static void markers_load_store(GtkTreeStore *markers_store, struct EventsDb *eventsdb)
{
	GtkTreeIter iter;
	struct markers_queue *m_queue;
	struct markers_queue_entry *m_entry;

	m_queue = EventsDb_GetMarkersQueue(eventsdb);
	for (m_entry = m_queue->tqh_first; m_entry != NULL; 
	     m_entry = m_entry->entries.tqe_next) {
		gtk_tree_store_append(markers_store, &iter, NULL);
		gtk_tree_store_set(markers_store, &iter,
			MARKERS_CHECK, TRUE,
			MARKERS_NAME, m_entry->marker,
			-1);
	}
}

static void markers_data_func(__attribute__((unused))GtkTreeViewColumn *column,
	GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, 
	__attribute__((unused))gpointer user_data)
{
	gboolean enabled;

	gtk_tree_model_get(model, iter, MARKERS_CHECK, &enabled, -1);
	gtk_cell_renderer_toggle_set_active(
		GTK_CELL_RENDERER_TOGGLE(renderer), enabled);
}

static int markers_count(GtkTreeModel *model)
{
	int count = 0;
	GtkTreeIter iter;
	gboolean enabled;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return 0;

	while(1) {
		gtk_tree_model_get(model, &iter, MARKERS_CHECK, &enabled, -1);
		if (enabled)
			count++;
		if (!gtk_tree_model_iter_next(model, &iter))
			break;
	}
	return count;
}

static void markers_create_enabled_list(GtkTreeModel *model, 
	char *markers_list[], int markers_list_length)
{
	GtkTreeIter iter;
	gboolean enabled;
	gchar *marker_name;
	int i = 0;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	while(1) {
		gtk_tree_model_get(model, &iter, MARKERS_CHECK, &enabled, -1);
		if (enabled) {
			gtk_tree_model_get(model, &iter, 
				MARKERS_NAME, &marker_name, -1);
			markers_list[i++] = marker_name;
			assert(i <= markers_list_length);
		}
		if (!gtk_tree_model_iter_next(model, &iter))
			break;
	}
}

static GtkTreeStore *events_init_store_types(int columns)
{
	GtkTreeStore *store;
	GType *store_entries_types;
	int i;

	store_entries_types = malloc(columns * sizeof(GType));
	assert(NULL != store_entries_types);

	for (i = 0; i < columns; i++)
		store_entries_types[i] = G_TYPE_STRING;

	store = gtk_tree_store_newv(columns, store_entries_types);

	free(store_entries_types);
	return store;
}

static void events_init_store_content(GtkTreeStore *store, struct EventsDb *eventsdb)
{
	size_t columns, rows, column_i, row_i;
	GtkTreeIter iter;
	const char *value;
	GValue *value_container;

	columns = EventsDb_ResponseGetColumns(eventsdb);
	rows = EventsDb_ResponseGetRows(eventsdb);
	for (row_i = 0; row_i < rows; row_i++) {
		gtk_tree_store_append(store, &iter, NULL);
		for (column_i = 0; column_i < columns; column_i++) 
		{
			value = EventsDb_ResponseGetValueAt(eventsdb, 
				column_i, row_i);
			value_container = malloc(sizeof(*value_container));
			assert(NULL != value_container);
			memset(value_container, 0, sizeof(*value_container));

			g_value_init(value_container, G_TYPE_STRING);
			g_value_set_static_string(value_container, 
				value != NULL ? value : "");
			gtk_tree_store_set_value(store, &iter, 
				column_i, value_container);
		}
	}
}

static GtkTreeStore *events_init_store(struct EventsDb *eventsdb, 
	GtkTreeModel *markers_model)
{
	enum EventsDb_Error err_evdb;
	GtkTreeStore *store;
	char **markers;
	size_t markers_length, markers_size;

	markers_length = markers_count(markers_model);
	markers_size = markers_length * sizeof(char *);
	markers = malloc(markers_size);
	assert(NULL != markers);

	markers_create_enabled_list(markers_model, markers, markers_length);
	EventsDb_ResponseFreeMemory(eventsdb);
	err_evdb = EventsDb_RequestEventsTable(eventsdb, markers, markers_length);
	assert(EVENTSDB_OK == err_evdb);
	eventsdb->response_markers_count = markers_length;
	store = events_init_store_types(EventsDb_ResponseGetColumns(eventsdb)); 
	events_init_store_content(store, eventsdb);
	
	return store;
}

static void events_init_view(GtkWidget *events_tree_view, struct EventsDb *eventsdb)
{
	GtkCellRenderer *render_text;
	GtkTreeViewColumn *column;
	size_t i, markers_count;
	char *column_name;

	markers_count = EventsDb_ResponseMarkersCount(eventsdb);
	render_text = gtk_cell_renderer_text_new();
	for (i = 0; i < markers_count + 1; i++) {
		column_name = (i == 0) ? 
			"Time" : EventsDb_ResponseMarkerAt(eventsdb, i - 1);
		column = gtk_tree_view_column_new_with_attributes(
			column_name, render_text, "text", i, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(events_tree_view),
			column);
	}
}

static GtkWidget *events_create_view(GtkWidget *parent, 
	GtkWidget *markers_tree_view, struct EventsDb *eventsdb)
{
	GtkTreeModel *model;
	GtkWidget *events_tree_view;
	GtkTreeStore *events_store;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(markers_tree_view));
	events_store = events_init_store(eventsdb, model);
	events_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(events_store));
	events_init_view(events_tree_view, eventsdb);
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(events_tree_view),
		GTK_TREE_VIEW_GRID_LINES_BOTH);
	gtk_container_add(GTK_CONTAINER(parent), events_tree_view);
	gtk_widget_show(events_tree_view);

	return events_tree_view;
}

void markers_toggled_cb(__attribute__((unused))GtkCellRendererToggle* renderer, 
	gchar* pathStr, gpointer user_data)
{
	struct Session *info;
	GtkTreePath* path;
	GtkTreeIter iter;
	GtkTreeModel *markers_model;
	gboolean enabled;

	info = (struct Session *)user_data;
	markers_model = gtk_tree_view_get_model(GTK_TREE_VIEW(info->markers_tree_view));

	path = gtk_tree_path_new_from_string(pathStr);
	gtk_tree_model_get_iter(markers_model, &iter, path);
	gtk_tree_model_get(markers_model, &iter, 
		MARKERS_CHECK, &enabled, 
		-1);

	enabled = !enabled;
	gtk_tree_store_set(GTK_TREE_STORE(markers_model), &iter, 
		MARKERS_CHECK, enabled, -1);

	gtk_widget_destroy(info->events_tree_view);
	info->events_tree_view = events_create_view(info->events_tree_view_parent,
		info->markers_tree_view, info->eventsdb);
}

static GtkWidget *markers_init_view(GtkTreeStore *markers_store, 
	struct Session *session)
{
	GtkCellRenderer *render_markers_check;
	GtkCellRenderer *render_markers_name;
	GtkTreeViewColumn *column_markers_check;
	GtkTreeViewColumn *column_markers_name;
	GtkWidget *markers_tree_view;

	markers_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(markers_store));

	render_markers_check = gtk_cell_renderer_toggle_new();
	column_markers_check = gtk_tree_view_column_new_with_attributes(
		"", GTK_CELL_RENDERER(render_markers_check), NULL);
	g_signal_connect(render_markers_check, "toggled", 
		(GCallback)markers_toggled_cb, session);
	gtk_tree_view_column_set_cell_data_func(
		column_markers_check, render_markers_check,
		markers_data_func, NULL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(markers_tree_view), 
		column_markers_check);

	render_markers_name = gtk_cell_renderer_text_new();
	column_markers_name= gtk_tree_view_column_new_with_attributes(
		"Markers", render_markers_name, "text", MARKERS_NAME, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(markers_tree_view), 
		column_markers_name);

	return markers_tree_view;
}

static GtkWidget *activate_markers_view(struct Session *info)
{
	GtkWidget *markers_view_scroll;
	GtkTreeStore *markers_store;

	markers_view_scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(markers_view_scroll);

	markers_store = gtk_tree_store_new(MARKERS_TOTAL, 
		G_TYPE_BOOLEAN, G_TYPE_STRING);
	markers_load_store(markers_store, info->eventsdb);

	info->markers_tree_view = markers_init_view(markers_store, info);
	gtk_container_add(GTK_CONTAINER(markers_view_scroll), 
		info->markers_tree_view);
	gtk_widget_show(info->markers_tree_view);

	return markers_view_scroll;
}

static GtkWidget *activate_events_view(struct Session *info)
{
	GtkWidget *events_list_scroll;

	events_list_scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(events_list_scroll);
	info->events_tree_view_parent = events_list_scroll;

	info->events_tree_view = events_create_view(info->events_tree_view_parent,
		info->markers_tree_view, info->eventsdb);

	return info->events_tree_view_parent;
}

static void gui_startup(
	__attribute__((unused))GtkApplication *app, 
	__attribute__((unused))gpointer user_data)
{
	;
}

static void gui_open_new(GtkApplication *app, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *main_panels;
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_maximize(GTK_WINDOW(window));
	
	main_panels = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add(GTK_CONTAINER(window), main_panels);
	gtk_widget_show(main_panels);

	gtk_paned_add1(GTK_PANED(main_panels), activate_markers_view(user_data));
	gtk_paned_add2(GTK_PANED(main_panels), activate_events_view(user_data));

	gtk_window_set_application(GTK_WINDOW(window), app);
	gtk_widget_show(window);
}

static void gui_open(GApplication *application, gpointer *files, gint n_files, 
	__attribute__((unused))gchar *hint, gpointer user_data)
{
	struct Session *info;
	gint i;

	assert(NULL != user_data);
	info = user_data;

	assert(NULL != files);
	for(i = 0; i < n_files; i++) {
		EventsDb_AddLog(info->eventsdb, g_file_get_path(files[i]));
		/* TODO: handle errors here */
	}
	/* TODO: change forced type conversion */
	gui_open_new((GtkApplication *)application, info);
}

int gui_main(int argc, char *argv[], struct EventsDb *eventsdb)
{
	struct Session info;
	GtkApplication *app;
	int status;

	app = gtk_application_new(APPLICATION_ID, G_APPLICATION_HANDLES_OPEN);
	assert(NULL != app);

	info.eventsdb = eventsdb;
 	g_signal_connect(app, "startup", G_CALLBACK(gui_startup), &info);
 	g_signal_connect(app, "open", G_CALLBACK(gui_open), &info);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
