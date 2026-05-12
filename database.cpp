#include "database.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

std::mutex g_dbMutex;
std::string g_dataDir;

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return w;
}

std::filesystem::path pathFromUtf8(const std::string& utf8) {
    return std::filesystem::path(utf8ToWide(utf8));
}
#else
std::filesystem::path pathFromUtf8(const std::string& utf8) {
    return std::filesystem::path(utf8);
}
#endif

struct Course {
    std::string code;
    std::string title;
    std::string section;
    std::string instructor;
    std::string day;
    std::string timeRange;
    std::string classroom;
    std::string semester;
};

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::vector<std::string> splitSpaces(const std::string& line) {
    std::istringstream iss(line);
    return std::vector<std::string>(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>());
}

std::string joinTail(const std::vector<std::string>& parts, std::size_t start) {
    if (start >= parts.size()) {
        return {};
    }
    std::string out = parts[start];
    for (std::size_t i = start + 1; i < parts.size(); ++i) {
        out.push_back(' ');
        out += parts[i];
    }
    return out;
}

std::string coursesPath() {
    if (g_dataDir.empty()) {
        return "courses.csv";
    }
    char last = g_dataDir.back();
    if (last == '\\' || last == '/') {
        return g_dataDir + "courses.csv";
    }
    return g_dataDir + "\\courses.csv";
}

std::string usersPath() {
    if (g_dataDir.empty()) {
        return "users.txt";
    }
    char last = g_dataDir.back();
    if (last == '\\' || last == '/') {
        return g_dataDir + "users.txt";
    }
    return g_dataDir + "\\users.txt";
}

// Minimal CSV: fields may be quoted; commas inside quotes allowed.
std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                out.push_back(trim(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
    }
    out.push_back(trim(cur));
    return out;
}

std::string escapeCsvField(const std::string& s) {
    if (s.find_first_of(",\"\r\n") != std::string::npos) {
        std::string e = "\"";
        for (char c : s) {
            if (c == '"') {
                e += "\"\"";
            } else {
                e.push_back(c);
            }
        }
        e += '"';
        return e;
    }
    return s;
}

void loadCourses(std::vector<Course>& out) {
    out.clear();
    std::ifstream in(pathFromUtf8(coursesPath()));
    if (!in) {
        return;
    }
    std::string header;
    if (!std::getline(in, header)) {
        return;
    }
    std::string row;
    while (std::getline(in, row)) {
        if (trim(row).empty()) {
            continue;
        }
        auto f = parseCsvLine(row);
        if (f.size() < 8) {
            continue;
        }
        Course c;
        c.code = f[0];
        c.title = f[1];
        c.section = f[2];
        c.instructor = f[3];
        c.day = f[4];
        c.timeRange = f[5];
        c.classroom = f[6];
        c.semester = f[7];
        out.push_back(std::move(c));
    }
}

void saveCourses(const std::vector<Course>& courses) {
    std::ofstream out(pathFromUtf8(coursesPath()), std::ios::trunc);
    if (!out) {
        return;
    }
    out << "code,title,section,instructor,day,time,classroom,semester\n";
    for (const auto& c : courses) {
        out << escapeCsvField(c.code) << ',' << escapeCsvField(c.title) << ',' << escapeCsvField(c.section) << ','
            << escapeCsvField(c.instructor) << ',' << escapeCsvField(c.day) << ',' << escapeCsvField(c.timeRange) << ','
            << escapeCsvField(c.classroom) << ',' << escapeCsvField(c.semester) << '\n';
    }
}

bool loadAdmin(const std::string& user, const std::string& pass) {
    std::ifstream in(pathFromUtf8(usersPath()));
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream iss(line);
        std::string u, p;
        if (!(iss >> u >> p)) {
            continue;
        }
        if (u == user && p == pass) {
            return true;
        }
    }
    return false;
}

std::string formatCourseLine(const Course& c) {
    std::ostringstream os;
    os << c.code << " | " << c.title << " | Sec " << c.section << " | " << c.instructor << " | " << c.day << " "
       << c.timeRange << " | " << c.classroom << " | " << c.semester;
    return os.str();
}

std::vector<Course> filterByCode(const std::vector<Course>& all, const std::string& code) {
    std::vector<Course> r;
    std::string uc = toUpper(code);
    for (const auto& c : all) {
        if (toUpper(c.code) == uc) {
            r.push_back(c);
        }
    }
    return r;
}

std::vector<Course> filterByInstructor(const std::vector<Course>& all, const std::string& name) {
    std::vector<Course> r;
    std::string un = toUpper(name);
    for (const auto& c : all) {
        if (toUpper(c.instructor).find(un) != std::string::npos) {
            r.push_back(c);
        }
    }
    return r;
}

std::vector<Course> filterBySemester(const std::vector<Course>& all, const std::string& sem) {
    if (trim(sem).empty()) {
        return all;
    }
    std::vector<Course> r;
    std::string us = toUpper(sem);
    for (const auto& c : all) {
        if (toUpper(c.semester) == us) {
            r.push_back(c);
        }
    }
    return r;
}

bool parseAddCsv(const std::string& payload, Course& c, std::string& err) {
    auto f = parseCsvLine(payload);
    if (f.size() != 8) {
        err = "ERROR ADD expects 8 comma-separated fields: code,title,section,instructor,day,time,classroom,semester";
        return false;
    }
    c.code = f[0];
    c.title = f[1];
    c.section = f[2];
    c.instructor = f[3];
    c.day = f[4];
    c.timeRange = f[5];
    c.classroom = f[6];
    c.semester = f[7];
    if (c.code.empty()) {
        err = "ERROR course code required";
        return false;
    }
    return true;
}

std::size_t findCourseIndex(std::vector<Course>& courses, const std::string& code, const std::string& section) {
    std::string uc = toUpper(code);
    std::string usec = toUpper(section);
    for (std::size_t i = 0; i < courses.size(); ++i) {
        if (toUpper(courses[i].code) == uc && (section.empty() || toUpper(courses[i].section) == usec)) {
            return i;
        }
    }
    return static_cast<std::size_t>(-1);
}

} // namespace

void setDatabaseDataDirectory(const std::string& dir) {
    std::lock_guard<std::mutex> lock(g_dbMutex);
    g_dataDir = trim(dir);
}

std::string handleRequest(const std::string& request, ClientSession& session) {
    std::lock_guard<std::mutex> lock(g_dbMutex);

    std::string line = trim(request);
    if (line.empty()) {
        return "ERROR empty request";
    }

    auto parts = splitSpaces(line);
    if (parts.empty()) {
        return "ERROR empty request";
    }

    std::string cmd = toUpper(parts[0]);

    if (cmd == "LOGIN") {
        if (parts.size() < 3) {
            return "FAILURE";
        }
        std::string user = parts[1];
        std::string pass = joinTail(parts, 2);
        if (loadAdmin(user, pass)) {
            session.isAdmin = true;
            session.username = user;
            return "SUCCESS";
        }
        session.isAdmin = false;
        session.username.clear();
        return "FAILURE";
    }

    if (cmd == "LOGOUT") {
        session.isAdmin = false;
        session.username.clear();
        return "OK";
    }

    std::vector<Course> courses;
    loadCourses(courses);

    if (cmd == "QUERY") {
        if (parts.size() < 2) {
            return "ERROR QUERY needs subcommand";
        }
        std::string sub = toUpper(parts[1]);
        if (sub == "CODE") {
            if (parts.size() < 3) {
                return "ERROR QUERY CODE needs course code";
            }
            std::string code = joinTail(parts, 2);
            auto hits = filterByCode(courses, code);
            if (hits.empty()) {
                return "RESULT NOT FOUND";
            }
            std::ostringstream os;
            os << "RESULT " << hits.size();
            for (const auto& c : hits) {
                os << '\n' << formatCourseLine(c);
            }
            return os.str();
        }
        if (sub == "INSTRUCTOR" || sub == "TEACHER") {
            if (parts.size() < 3) {
                return "ERROR QUERY INSTRUCTOR needs name";
            }
            std::string name = joinTail(parts, 2);
            auto hits = filterByInstructor(courses, name);
            if (hits.empty()) {
                return "RESULT NOT FOUND";
            }
            std::ostringstream os;
            os << "RESULT " << hits.size();
            for (const auto& c : hits) {
                os << '\n' << formatCourseLine(c);
            }
            return os.str();
        }
        if (sub == "ALL") {
            std::string sem;
            if (parts.size() >= 3) {
                sem = joinTail(parts, 2);
            }
            auto base = filterBySemester(courses, sem);
            if (base.empty()) {
                return "RESULT NOT FOUND";
            }
            std::ostringstream os;
            os << "RESULT " << base.size();
            for (const auto& c : base) {
                os << '\n' << formatCourseLine(c);
            }
            return os.str();
        }
        return "ERROR unknown QUERY subcommand";
    }

    if (cmd == "ADD") {
        if (!session.isAdmin) {
            return "ERROR unauthorized";
        }
        if (parts.size() < 2 || toUpper(parts[1]) != "COURSE") {
            return "ERROR use ADD COURSE <csv fields>";
        }
        // Everything after "ADD COURSE " as payload
        std::size_t pos = line.find("course");
        if (pos == std::string::npos) {
            pos = line.find("COURSE");
        }
        if (pos == std::string::npos) {
            return "ERROR malformed ADD";
        }
        pos += 6;
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
            ++pos;
        }
        std::string payload = line.substr(pos);
        Course c;
        std::string err;
        if (!parseAddCsv(payload, c, err)) {
            return err;
        }
        for (const auto& ex : courses) {
            if (toUpper(ex.code) == toUpper(c.code) && toUpper(ex.section) == toUpper(c.section) &&
                toUpper(ex.semester) == toUpper(c.semester)) {
                return "ERROR duplicate course+section+semester";
            }
        }
        courses.push_back(std::move(c));
        saveCourses(courses);
        return "OK";
    }

    if (cmd == "UPDATE") {
        if (!session.isAdmin) {
            return "ERROR unauthorized";
        }
        // UPDATE CODE [SECTION sec] FIELD value...
        // Or: UPDATE CODE FIELD value (first matching code)
        if (parts.size() < 4) {
            return "ERROR UPDATE needs code and field";
        }
        std::string code = parts[1];
        std::size_t idx = 2;
        std::string sectionFilter;
        if (toUpper(parts[2]) == "SECTION" && parts.size() >= 5) {
            sectionFilter = parts[3];
            idx = 4;
        }
        if (parts.size() <= idx) {
            return "ERROR UPDATE needs field and value";
        }
        std::string field = toUpper(parts[idx]);
        std::string value;
        {
            std::size_t cut = 0;
            for (std::size_t i = 0; i < idx; ++i) {
                cut = line.find(parts[i], cut);
                if (cut == std::string::npos) {
                    break;
                }
                cut += parts[i].size();
            }
            // Skip to after field token
            std::size_t fpos = 0;
            for (std::size_t k = 0; k <= idx; ++k) {
                fpos = line.find(parts[k], fpos);
                if (fpos == std::string::npos) {
                    return "ERROR internal";
                }
                fpos += parts[k].size();
            }
            while (fpos < line.size() && std::isspace(static_cast<unsigned char>(line[fpos]))) {
                ++fpos;
            }
            value = trim(line.substr(fpos));
        }
        if (value.empty()) {
            return "ERROR UPDATE needs value";
        }

        std::size_t ci = findCourseIndex(courses, code, sectionFilter);
        if (ci == static_cast<std::size_t>(-1)) {
            return "ERROR not found";
        }

        if (field == "TITLE") {
            courses[ci].title = value;
        } else if (field == "SECTION") {
            courses[ci].section = value;
        } else if (field == "INSTRUCTOR" || field == "TEACHER") {
            courses[ci].instructor = value;
        } else if (field == "DAY") {
            courses[ci].day = value;
        } else if (field == "TIME") {
            courses[ci].timeRange = value;
        } else if (field == "CLASSROOM" || field == "ROOM") {
            courses[ci].classroom = value;
        } else if (field == "SEMESTER") {
            courses[ci].semester = value;
        } else if (field == "CODE") {
            courses[ci].code = value;
        } else {
            return "ERROR unknown field";
        }
        saveCourses(courses);
        return "OK";
    }

    if (cmd == "DELETE") {
        if (!session.isAdmin) {
            return "ERROR unauthorized";
        }
        // DELETE CODE [SECTION sec]
        if (parts.size() < 2) {
            return "ERROR DELETE needs code";
        }
        std::string code = parts[1];
        std::string sectionFilter;
        if (parts.size() >= 4 && toUpper(parts[2]) == "SECTION") {
            sectionFilter = parts[3];
        }
        std::size_t ci = findCourseIndex(courses, code, sectionFilter);
        if (ci == static_cast<std::size_t>(-1)) {
            return "ERROR not found";
        }
        courses.erase(courses.begin() + static_cast<std::ptrdiff_t>(ci));
        saveCourses(courses);
        return "OK";
    }

    return "ERROR unknown command";
}
