#pragma once

#include <string>

/// Per-client state for authentication. Member 1 should keep one ClientSession per TCP connection.
struct ClientSession {
    bool isAdmin = false;
    std::string username;
};

// --- Text protocol (one line per handleRequest unless noted) ---
// LOGIN <user> <password>     -> SUCCESS | FAILURE
// LOGOUT                        -> OK
// QUERY CODE <code>             -> RESULT NOT FOUND | multi-line RESULT (first line: count)
// QUERY INSTRUCTOR <name>       -> same as CODE (substring match, case-insensitive)
// QUERY ALL [semester]          -> all courses, optional semester filter (e.g. 2026S2)
// ADD COURSE <csv>              -> OK | ERROR ... (admin only; 8 fields, see courses.csv header)
// UPDATE <code> [SECTION <sec>] <FIELD> <value...>  -> OK | ERROR ... (admin)
//   FIELD: TITLE|SECTION|INSTRUCTOR|DAY|TIME|CLASSROOM|SEMESTER|CODE
// DELETE <code> [SECTION <sec>] -> OK | ERROR ... (admin)

/// Application-layer request handler for the timetable database.
/// \param request One line of text (no trailing \\r\\n required); UTF-8 recommended.
/// \param session Session for this connection; updated on successful LOGIN.
/// \return Response line(s) separated by \\n; last line has no trailing \\n unless empty.
std::string handleRequest(const std::string& request, ClientSession& session);

/// Optional: set data file directory (trailing slash optional). Default: current working directory.
void setDatabaseDataDirectory(const std::string& dir);
