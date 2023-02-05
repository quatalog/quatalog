#include<set>
#include<regex>
#include<fstream>
#include<iostream>
#include<filesystem>
#include<unordered_set>
#include<dist/jsoncpp.cpp>

namespace fs = std::filesystem;
using course_handler_t = void(const Json::Value&,const std::string&,Json::Value&);
void handle_term(const fs::directory_entry&,Json::Value&);
void handle_term_dirs(const std::set<fs::directory_entry>&,Json::Value&);
void handle_prefix(const Json::Value&,const std::string&,Json::Value&,course_handler_t*);
void handle_course(const Json::Value&,const std::string&,Json::Value&);
void handle_course_summer(const Json::Value&,const std::string&,Json::Value&);
void handle_attribute(const std::string&,Json::Value&);
void handle_attributes(const Json::Value&,Json::Value&);
void handle_sections(const Json::Value&,Json::Value&);
void handle_instructors(const Json::Value&,std::unordered_set<std::string>&);
void handle_multiple_instructors(const std::string&,std::unordered_set<std::string>&);

int main(const int argc,const char** argv) {
        if(argc < 2) {
                std::cerr << "Bad number of arguments " << argc << std::endl;
                return EXIT_FAILURE;
        }

        const auto data_dir_path = fs::path(argv[1]);
        
        if(!fs::is_directory(data_dir_path)) {
                std::cerr << "Data dir argument " << data_dir_path << " is not a directory" << std::endl;
                return EXIT_FAILURE;
        }

        // Sort term dirs chronologically using a std::set
        std::set<fs::directory_entry> term_dirs;
        const auto data_dir = fs::directory_iterator(data_dir_path);
        for(const auto& term : data_dir) {
                term_dirs.insert(term);
        }

        // Begin JSON manipulation
        Json::Value terms_offered;
        terms_offered["all_terms"] = Json::arrayValue;
        handle_term_dirs(term_dirs,terms_offered);

        std::cout << terms_offered << std::endl;

        return EXIT_SUCCESS;
}

void handle_term_dirs(const std::set<fs::directory_entry>& term_dirs,
                      Json::Value& terms_offered) {
        for(auto term : term_dirs) {
                if(!fs::is_directory(term)) {
                        continue;
                }

                handle_term(term,terms_offered);
        }

}

void handle_term(const fs::directory_entry& term_entry,
                       Json::Value& terms_offered) {
        const fs::path dir = term_entry.path();
        const auto dirname = dir.string();
        const auto term = dir.stem().string();
        const auto courses_filename = dirname + "/courses.json";
        const auto prereqs_filename = dirname + "/prerequisites.json";
        std::fstream courses_file{courses_filename,std::ios::in};
        std::fstream prereqs_file{prereqs_filename,std::ios::in};

        std::cerr << "Processing term " << term << "..." << std::endl;
        terms_offered["all_terms"].append(term);

        Json::Value courses;
        courses_file >> courses;

        course_handler_t* course_handler;
        if(term.substr(4,2) == "05") {
                course_handler = handle_course_summer;
        } else {
                course_handler = handle_course;
        }
        for(auto& prefix : courses) {
                handle_prefix(prefix,term,terms_offered,course_handler);
        }
}

void handle_prefix(const Json::Value& prefix,
                   const std::string& term,
                   Json::Value& terms_offered,
                   course_handler_t course_handler) {
#ifdef DEBUG
        std::cerr << "\tProcessing prefix "
                  << prefix["code"]
                  << " - "
                  << prefix["name"]
                  << "..."
                  << std::endl;
#endif
        for(auto& course : prefix["courses"]) {
                course_handler(course,term,terms_offered);
        }
}

void handle_course(const Json::Value& course,
                   const std::string& term,
                   Json::Value& terms_offered) {
        const auto course_code = course["id"].asString();
        
        auto& course_term = terms_offered[course_code][term];
        course_term["title"] = course["title"];

        const Json::Value& sections = course["sections"];

        handle_sections(sections,course_term);

        handle_attributes(course["sections"][0],course_term);
}

void handle_course_summer(const Json::Value& course,
                          const std::string& term,
                          Json::Value& terms_offered) {
        const auto course_code = course["id"].asString();

        // sections[0]: Full term sections
        // sections[1]: First-half term sections
        // sections[2]: Second-half term sections
        // Of course, a course will never be offered
        // in both the full term _and_ one of the
        // half-terms, but there are a few that
        // are offered in both halves (e.g. STSO-4100)
        std::array<Json::Value,3> sections;

        // We will loop twice over the sections,
        // once here and once in handle_sections,
        // but frankly I tried to make it all in 1
        // loop and the code was a total unreadable
        // mess. So I don't really care
        int subterm;
        bool subterm0 = false, subterm1 = false, subterm2 = false;
        for(const auto& section : course["sections"]) {
                const auto& timeslot = section["timeslots"][0];
                const std::string& dateEnd = timeslot["dateEnd"].asString();
                const std::string& dateStart = timeslot["dateStart"].asString();
                subterm = 0;
                if(dateStart.substr(0,2) != "05") {
                        subterm = 1;
                        subterm1 = true;
                } else if(dateEnd.substr(0,2) != "08") {
                        subterm = 2;
                        subterm2 = true;
                } else {
                        subterm0 = true;
                }
                sections[subterm].append(section);
        }

        auto& course_terms = terms_offered[course_code];
        
        // Forgive the repeated code; it's not that much of a mess
        if(subterm0) {
                Json::Value& course_term = course_terms[term];
                course_term["title"] = course["title"];
                handle_sections(sections[0],course_term);
                handle_attributes(sections[0][0],course_term);
        } else {
                if(subterm1) {
                        Json::Value& course_term = course_terms[term+"02"];
                        course_term["title"] = course["title"];
                        handle_sections(sections[1],course_term);
                        handle_attributes(sections[1][0],course_term);
                } if(subterm2) {
                        Json::Value& course_term = course_terms[term+"03"];
                        course_term["title"] = course["title"];
                        handle_sections(sections[2],course_term);
                        handle_attributes(sections[2][0],course_term);
                }
        }
}

void handle_sections(const Json::Value& sections,
                     Json::Value& course_term) {
        int credMin = Json::Value::maxInt, credMax = 0;
        int seatsTaken = 0, capacity = 0, remaining = 0;
        std::unordered_set<std::string> instructors;
        for(const auto& section : sections) {
                // Get min/max credits *of all sections*
                // (RCOS looking at you)
                credMin = std::min(credMin,section["credMin"].asInt());
                credMax = std::max(credMax,section["credMax"].asInt());
                
                // Add seating data of all sections together.
                // remaining might get clobbered by some sections
                // having negative seats, but this probably won't
                // be too much of an issue
                seatsTaken += section["act"].asInt();
                capacity   += section["cap"].asInt();
                remaining  += section["rem"].asInt();

                handle_instructors(section,instructors);
        }

        course_term["credits"]["min"]     = credMin;
        course_term["credits"]["max"]     = credMax;
        course_term["seats"]["taken"]     = seatsTaken;
        course_term["seats"]["capacity"]  = capacity;
        course_term["seats"]["remaining"] = remaining;
        for(const auto& instructor : instructors) {
                course_term["instructors"].append(instructor);
        }
}

void handle_instructors(const Json::Value& section,
                        std::unordered_set<std::string>& instructors) {
        for(const auto& timeslot : section["timeslots"]) {
                handle_multiple_instructors(timeslot["instructor"].asString(),instructors);
        }
}

void handle_multiple_instructors(const std::string& instructor_str,
                                 std::unordered_set<std::string>& instructors) {
        // Pseudo-string split on commas
        const auto delim = std::regex(", ?");
        const auto end_itr = std::sregex_token_iterator();
        auto instructors_itr = std::sregex_token_iterator(
                                       instructor_str.begin(),
                                       instructor_str.end(),
                                       delim,
                                       -1
                               );

        for(;instructors_itr != end_itr
          && !instructors_itr->str().empty();
             instructors_itr++) {
                const std::string& str = instructors_itr->str();
                if(str == "TBA") continue;
                instructors.insert(str);
        }
}

void handle_attributes(const Json::Value& section,
                       Json::Value& course_term) {
        // This mess is basically C++'s string split but not using
        // as much memory as an actual string split
        const auto delim = std::regex(" and |, ");
        const auto attributes_str = section["attribute"].asString();
        const auto end_itr = std::sregex_token_iterator();
        auto attributes_itr = std::sregex_token_iterator(
                                      attributes_str.begin(),
                                      attributes_str.end(),
                                      delim,
                                      -1
                              );

        // Makes the JSON list of attributes
        Json::Value& attributes = course_term["attributes"];
        attributes = Json::arrayValue;
        for(;attributes_itr != end_itr
          && !attributes_itr->str().empty();
             attributes_itr++) {
                handle_attribute(attributes_itr->str(),attributes);
        }
}

void handle_attribute(const std::string& attribute,
                      Json::Value& attributes) {
        if(attribute == "Communication Intensive") {
                attributes.append("[CI]");
        } else if(attribute == "Writing Intensive") {
                attributes.append("[WI]");
        } else if(attribute == "HASS Inquiry") {
                attributes.append("[HInq]");
        } else if(attribute == "Culminating Exp/Capstone") {
                attributes.append("[CulmExp]");
        } else if(attribute == "PDII Option for Engr Majors") {
                attributes.append("[PDII]");
        }
}
