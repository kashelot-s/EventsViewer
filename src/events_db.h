#ifndef __EVENTS_DB__
#define __EVENTS_DB__

#include <regex.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/queue.h>

#define EVENTSDB_PATTERN_LINE_VALID      "^UVM_INFO.*@.*"
#define EVENTSDB_PATTERN_EXTRACT_MARKER  "\\[.*\\]"
#define EVENTSDB_PATTERN_EXTRACT_TIME    "@ [0-9]*"
#define EVENTSDB_PATTERN_EXTRACT_MESSAGE "\\].*$"

enum EventsDb_Error{
	EVENTSDB_OK,
	EVENTSDB_CANT_OPEN,
	EVENTSDB_MALFORMED_LINE,
	EVENTSDB_PATTERN_LINE_VALID_WRONG,
	EVENTSDB_PATTERN_EXTRACT_WRONG,
	EVENTSDB_NOT_ENOUGHT_MEM
};

TAILQ_HEAD(markers_queue, markers_queue_entry);
struct markers_queue_entry {
	TAILQ_ENTRY(markers_queue_entry) entries;
	const char *marker;
};

TAILQ_HEAD(time_queue, time_queue_entry);
struct time_queue_entry{
	TAILQ_ENTRY(time_queue_entry) entries;
	const char *time;
};

TAILQ_HEAD(events_queue, events_queue_entry);
struct events_queue_entry {
	TAILQ_ENTRY(events_queue_entry) entries;
	const char *marker;
	const char *time;
	const char *message;
};

struct EventsDb {
	regex_t regex_line_valid;
	regex_t regex_extract_marker;
	regex_t regex_extract_time;
	regex_t regex_extract_message;

	struct markers_queue m_queue;
	struct time_queue t_queue;
	int t_queue_length;
	struct events_queue e_queue;

	bool response_valid;

	char **response;
	size_t response_rows;
	size_t response_columns;

	char **response_markers;
	size_t response_markers_count;
};

enum EventsDb_Error EventsDb_Init(struct EventsDb *eventsdb);

void EventsDb_Done(struct EventsDb *eventsdb);

enum EventsDb_Error EventsDb_NewPatterns(struct EventsDb *eventsdb,
	const char *pattern_line_valid, 
	const char *pattern_extract_marker,
	const char *pattern_extract_time,
	const char *pattern_extract_message);

enum EventsDb_Error EventsDb_AddLog(struct EventsDb *eventsdb, const char *log_name);

struct markers_queue *EventsDb_GetMarkersQueue(struct EventsDb *eventsdb);


enum EventsDb_Error EventsDb_RequestEventsTable(struct EventsDb *eventsdb, 
	char *markers[], size_t markers_length);

char *EventsDb_ResponseGetValueAt(struct EventsDb *eventsdb, size_t column, size_t row);
char *EventsDb_ResponseMarkerAt(struct EventsDb *eventsdb, size_t index);

size_t EventsDb_ResponseGetColumns(struct EventsDb *eventsdb);
size_t EventsDb_ResponseGetRows(struct EventsDb *eventsdb);
size_t EventsDb_ResponseMarkersCount(struct EventsDb *eventsdb);
void EventsDb_ResponseFreeMemory(struct EventsDb *eventsdb);

#endif
