#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "events_db.h"

#define EVENTSDB_LINE_BUFFER_SIZE 1024

enum EventsDb_Error EventsDb_Init(struct EventsDb *eventsdb){
	enum EventsDb_Error err;

	memset(eventsdb, 0, sizeof(*eventsdb));
	err = EventsDb_NewPatterns(eventsdb, EVENTSDB_PATTERN_LINE_VALID,
		EVENTSDB_PATTERN_EXTRACT_MARKER,
		EVENTSDB_PATTERN_EXTRACT_TIME,
		EVENTSDB_PATTERN_EXTRACT_MESSAGE);
	if(err) return err;

	TAILQ_INIT(&eventsdb->m_queue);
	TAILQ_INIT(&eventsdb->e_queue);
	TAILQ_INIT(&eventsdb->t_queue);
	eventsdb->t_queue_length = 0;

	eventsdb->response = NULL;
	eventsdb->response_rows = 0;
	eventsdb->response_columns = 0;

	return EVENTSDB_OK;
}

enum EventsDb_Error EventsDb_NewPatterns(struct EventsDb *eventsdb,
	const char *pattern_line_valid, 
	const char *pattern_extract_marker,
	const char *pattern_extract_time,
	const char *pattern_extract_message)
{
	int err;

	err = regcomp(&eventsdb->regex_line_valid, pattern_line_valid, 0);
	if(err) return EVENTSDB_PATTERN_LINE_VALID_WRONG;

	err = regcomp(&eventsdb->regex_extract_marker, pattern_extract_marker, 
		REG_EXTENDED);
	if(err) return EVENTSDB_PATTERN_EXTRACT_WRONG;

	err = regcomp(&eventsdb->regex_extract_time, pattern_extract_time, 
		REG_EXTENDED);
	if(err) return EVENTSDB_PATTERN_EXTRACT_WRONG;

	err = regcomp(&eventsdb->regex_extract_message, pattern_extract_message, 
		REG_EXTENDED);
	if(err) return EVENTSDB_PATTERN_EXTRACT_WRONG;

	return EVENTSDB_OK;
}

static void EventsDb_AddEventBody(struct EventsDb *eventsdb, const char *marker,
	const char *time, const char *message)
{
	struct events_queue_entry *entry;

	entry = malloc(sizeof(*entry));
	assert(NULL != entry);

	entry->marker = marker;
	entry->time = time;
	entry->message = message;

	TAILQ_INSERT_TAIL(&eventsdb->e_queue, entry, entries);
}

static void EventsDb_AddMarker(struct EventsDb *eventsdb, const char *marker)
{
	struct markers_queue_entry *entry;

	entry = malloc(sizeof(*entry));
	assert(NULL != entry);
	entry->marker = marker;
	TAILQ_INSERT_TAIL(&eventsdb->m_queue, entry, entries);
}

static void EventsDb_AddTime(struct EventsDb *eventsdb, const char *time)
{
	struct time_queue_entry *entry;

	entry = malloc(sizeof(*entry));
	assert(NULL != entry);
	entry->time = time;
	TAILQ_INSERT_TAIL(&eventsdb->t_queue, entry, entries);
	eventsdb->t_queue_length += 1;
}

static bool EventsDb_IsMarkerExists(struct EventsDb *eventsdb, const char *marker)
{
	struct markers_queue_entry *np;

	for (np = eventsdb->m_queue.tqh_first; np != NULL; np = np->entries.tqe_next)
		if (0 == strcmp(np->marker, marker))
			return true;
	return false;
}

static void EventsDb_AddEvent(struct EventsDb *eventsdb, 
	const char *marker, const char *time, const char *message)
{
	// printf("AddEvent |%s|%s|%s|\n", time, marker, message);

	if (!EventsDb_IsMarkerExists(eventsdb, marker))
		EventsDb_AddMarker(eventsdb, marker);

	EventsDb_AddTime(eventsdb, time);

	EventsDb_AddEventBody(eventsdb, marker, time, message);
}

static bool EventsDb_ParseLineValid(struct EventsDb *eventsdb, const char *buffer)
{
	return 0 == regexec(&eventsdb->regex_line_valid, buffer, 0, NULL, 0);
}

static void EventsDb_CleanString(char *buffer)
{
	size_t i = 0;

	while(1) {
		if (buffer[i] == 0)
			break;
		if (buffer[i] == '\n' || 
		    buffer[i] == ']'  ||
		    buffer[i] == '[') {
			buffer[i] = ' ';	
			continue;
		}
		i++;
	}
}

static void EventsDb_MatchToBuffer(const char *buffer, 
	regmatch_t *result, char **result_buffer)
{
	size_t length = result->rm_eo - result->rm_so;
	char *result_buffer_int;

	result_buffer_int = malloc(length + 1);
	strncpy(result_buffer_int, &buffer[result->rm_so], length);
	result_buffer_int[length] = 0;
	EventsDb_CleanString(result_buffer_int);
	*result_buffer = result_buffer_int;
}

static enum EventsDb_Error EventsDb_ParseLineExtract(regex_t *expr, 
	const char *buffer, char **result)
{
	int match_status;
	enum EventsDb_Error err = EVENTSDB_OK;
	size_t n_match = 1;
	regmatch_t matches[n_match];

	match_status = regexec(expr, buffer, n_match, matches, 0);
	if (match_status) {
		printf("malformed line: %s", buffer);
		err = EVENTSDB_MALFORMED_LINE;
		goto out;
	}
	EventsDb_MatchToBuffer(buffer, &matches[0], result);
out:
	return err;
}

static enum EventsDb_Error EventsDb_ParseLine(
	struct EventsDb *eventsdb, const char *buffer)
{
	enum EventsDb_Error err;
	char *marker = NULL;
	char *time = NULL;
	char *message = NULL;

	if (!EventsDb_ParseLineValid(eventsdb, buffer))
		return EVENTSDB_OK;

	err = EventsDb_ParseLineExtract(&eventsdb->regex_extract_marker, 
		buffer, &marker);
	if (err) return err;

	err = EventsDb_ParseLineExtract(&eventsdb->regex_extract_time, 
		buffer, &time);
	if (err) return err;

	err = EventsDb_ParseLineExtract(&eventsdb->regex_extract_message, 
		buffer, &message);
	if (err) return err;

	EventsDb_AddEvent(eventsdb, marker, time, message);

	return EVENTSDB_OK;
}

enum EventsDb_Error EventsDb_AddLog(struct EventsDb *eventsdb, const char *log_name)
{
	FILE *log;
	ssize_t read;
	char *buffer;
	size_t buffer_size = EVENTSDB_LINE_BUFFER_SIZE;

	log = fopen(log_name, "r");
	if (NULL == log)
		return EVENTSDB_CANT_OPEN;	

	buffer = malloc(buffer_size);
	assert(NULL != buffer);
	while(1) {
		read = getline(&buffer, &buffer_size, log);
		if (-1 == read)
			break;
		EventsDb_ParseLine(eventsdb, buffer);
	}

	fclose(log);
	free(buffer);
	return EVENTSDB_OK;
}

struct markers_queue *EventsDb_GetMarkersQueue(struct EventsDb *eventsdb)
{
	return &eventsdb->m_queue;
}

static char *EventsDb_MarkerAtTime(struct EventsDb *eventsdb, 
	const char *marker, const char *time, size_t enter_n)
{
	struct events_queue_entry *np;
	size_t same_counter = 0;

	for (np = eventsdb->e_queue.tqh_first; np != NULL; np = np->entries.tqe_next)
		if (0 == strcmp(np->marker, marker) && 
		    0 == strcmp(np->time, time)) 
		{
			if (0 == enter_n) 
			{
				return (char *)np->message;
			} else {
				if (same_counter == enter_n)
					return (char *)np->message;
				same_counter++;
			}
		}
	return NULL;
}

enum EventsDb_Error EventsDb_ResponseAllocateMemory(struct EventsDb *eventsdb, 
	size_t markers_length)
{
	size_t response_size, entries;

	eventsdb->response_columns = markers_length + 1; /* +1 for timestamp field */
	eventsdb->response_rows = eventsdb->t_queue_length;

	entries = eventsdb->response_columns * eventsdb->response_rows;
	response_size = entries * sizeof(char *);
	eventsdb->response = malloc(response_size);
	if (NULL == eventsdb->response)
		return EVENTSDB_NOT_ENOUGHT_MEM;

	return EVENTSDB_OK;
}

static char *EventsDb_GetStubMessage(void)
{
	char *stub_str;

	stub_str = malloc(2);
	assert(NULL != stub_str);
	stub_str[0] = ' ';
	stub_str[1] = '\0';
	return stub_str;
}

static bool EventsDb_ResponseContentOneLine(struct EventsDb *eventsdb,
	char *markers[], size_t markers_length, 
	struct time_queue_entry *np, size_t current_row, size_t enter_n)
{
	size_t i, table_i;
	bool all_null = true;
	char *message;

	if (0 == markers_length)
		goto out;

	for (i = 0; i < markers_length; i++) {
		message = EventsDb_MarkerAtTime(eventsdb, markers[i], np->time, enter_n);
		table_i = current_row * eventsdb->response_columns + i + 1;
		if (NULL != message) {
			eventsdb->response[table_i] = message;
			all_null = false;
		} else
			message = EventsDb_GetStubMessage();
		assert(message != NULL);
		eventsdb->response[table_i] = message;
	}

out:
	return all_null;
}

static enum EventsDb_Error EventsDb_ResponseContent(struct EventsDb *eventsdb, 
	char *markers[], size_t markers_length)
{
	struct time_queue_entry *np;
	size_t j, table_i;
	bool all_null;
	const char *previous_time = "";
	size_t same_counter = 0;
	size_t response_rows_limit;

	response_rows_limit = eventsdb->response_rows;
	eventsdb->response_rows = 0;
	for (np = eventsdb->t_queue.tqh_first, j = 0; np != NULL; 
	     np = np->entries.tqe_next) {
		if (0 == strcmp(np->time, previous_time))
			same_counter += 1;
		else
			same_counter = 0;
		all_null = EventsDb_ResponseContentOneLine(eventsdb, 
			markers, markers_length, np, j, same_counter);
		if (!all_null || 0 == markers_length) {
			table_i = j * eventsdb->response_columns;
			eventsdb->response[table_i] = (char *)np->time;
			j++;
			assert(eventsdb->response_rows < response_rows_limit);
			eventsdb->response_rows++;
		}
		previous_time = np->time;
	}

	return EVENTSDB_OK;
}

enum EventsDb_Error EventsDb_ResponseCopyMarkers(struct EventsDb *eventsdb,
	char *markers[], size_t markers_length)
{
	size_t markers_size, i;

	eventsdb->response_markers_count = markers_length;
	markers_size = markers_length * sizeof(char *);
	eventsdb->response_markers = malloc(markers_size);
	if (NULL == eventsdb->response_markers)
		return EVENTSDB_NOT_ENOUGHT_MEM;
	
	for (i = 0; i < markers_length; i++)
		eventsdb->response_markers[i] = markers[i];

	return EVENTSDB_OK;
}

enum EventsDb_Error EventsDb_RequestEventsTable(struct EventsDb *eventsdb, 
	char *markers[], size_t markers_length)
{
	enum EventsDb_Error err;

	err = EventsDb_ResponseCopyMarkers(eventsdb, markers, markers_length);
	if (err) return err;

	err = EventsDb_ResponseAllocateMemory(eventsdb, markers_length);
	if (err) return err;

	err = EventsDb_ResponseContent(eventsdb, markers, markers_length);
	if (err) return err;

	eventsdb->response_valid = true;

	return EVENTSDB_OK;
}

char *EventsDb_ResponseGetValueAt(struct EventsDb *eventsdb, size_t column, size_t row)
{
	size_t index;
	
	assert(column < eventsdb->response_columns);
	assert(row < eventsdb->response_rows);
	index = row * eventsdb->response_columns + column;
	return eventsdb->response[index];
}

size_t EventsDb_ResponseGetColumns(struct EventsDb *eventsdb)
{
	return eventsdb->response_columns;
}

size_t EventsDb_ResponseGetRows(struct EventsDb *eventsdb)
{
	return eventsdb->response_rows;
}

char *EventsDb_ResponseMarkerAt(struct EventsDb *eventsdb, size_t index)
{
	assert(index < eventsdb->response_markers_count);
	return eventsdb->response_markers[index];
}

size_t EventsDb_ResponseMarkersCount(struct EventsDb *eventsdb)
{
	return eventsdb->response_markers_count;
}

void EventsDb_ResponseFreeMemory(struct EventsDb *eventsdb)
{
	if(!eventsdb->response_valid)
		return;

	free(eventsdb->response);
	free(eventsdb->response_markers);
	eventsdb->response_valid = false;
}

void EventsDb_Done(__attribute__((unused))struct EventsDb *eventsdb)
{
	/* TODO: free memory for all stored events */
	/* regfree */
	/* result */
}

