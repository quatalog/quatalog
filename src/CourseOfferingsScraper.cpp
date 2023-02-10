#include<set>
#include<regex>
#include<fstream>
#include<iostream>
#include<filesystem>
#include<unordered_set>
#include<json/json.h>
namespace fs = std::filesystem;

struct term_data_t {
        Json::Value courses;
        Json::Value prerequisites;
};
struct quatalog_data_t {
        Json::Value terms_offered;
        Json::Value prerequisites;
        Json::Value list_of_terms;
};
using course_handler_t = void(const Json::Value&,const std::string&,quatalog_data_t&,const Json::Value&);

void handle_term_dirs(const std::set<fs::directory_entry>&,quatalog_data_t&);
void handle_term(const fs::directory_entry& term_entry,quatalog_data_t&);
void handle_prefix(const Json::Value&,const std::string&,quatalog_data_t&,const term_data_t&,course_handler_t*);
void handle_course(const Json::Value&,const std::string&,quatalog_data_t&,const Json::Value&);
void handle_course_summer(const Json::Value&,const std::string&,quatalog_data_t&,const Json::Value&);
void handle_everything(const Json::Value&,const Json::Value&,const std::string&,Json::Value& course_term,Json::Value&,const Json::Value&);
void handle_sections(const Json::Value&,Json::Value&);
void handle_instructors(const Json::Value&,std::unordered_set<std::string>&);
void handle_multiple_instructors(const std::string&,std::unordered_set<std::string>&);
void handle_attributes(const Json::Value&,const std::string&,Json::Value&,Json::Value&);
void handle_term_attribute(const std::string&,Json::Value&);
void handle_attribute(const std::string&,Json::Value&,Json::Value&);
template<typename Functor> void iterateOnDelimitedString(const std::string&,const std::regex&,const Functor&);
void handle_prereqs(const Json::Value&,const std::string&,Json::Value&,const Json::Value&);

int main(const int argc,
         const char** argv) {
        if(argc != 5) {
                std::cerr << "Bad number of arguments (" << argc << ")" << std::endl;
                std::cerr << "Usage: " << argv[0]
                          << " <data_directory>"
                          << " <terms_offered_file>"
                          << " <prerequisites_file>"
                          << " <list_of_terms_file>"
                          << std::endl;
                return EXIT_FAILURE;
        }

        const auto& data_dir_path = fs::path(argv[1]);
        const auto& terms_offered_filename = std::string(argv[2]);
        const auto& prerequisites_filename = std::string(argv[3]);
        const auto& list_of_terms_filename = std::string(argv[4]);

        if(!fs::is_directory(data_dir_path)) {
                std::cerr << "Data directory argument "
                          << data_dir_path
                          << " is not a directory" << std::endl;
                return EXIT_FAILURE;
        }

        // Sort term dirs chronologically using a std::set
        std::set<fs::directory_entry> term_dirs;
        const auto& data_dir = fs::directory_iterator(data_dir_path);
        for(const auto& term : data_dir) {
                term_dirs.insert(term);
        }

        // Begin JSON manipulation
        quatalog_data_t data;

        // TODO: Once change to QuACS that accounts for prerelease data
        // is merged, change this
        data.list_of_terms["oldest_term"] = term_dirs.begin()->path().stem().string();
        data.list_of_terms["current_term"] = term_dirs.rbegin()->path().stem().string();
        
        handle_term_dirs(term_dirs,data);

        Json::StreamWriterBuilder swb;
        swb["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> outWriter(swb.newStreamWriter());

        std::fstream terms_offered_file{terms_offered_filename,std::ios::out};
        std::fstream prerequisites_file{prerequisites_filename,std::ios::out};
        std::fstream list_of_terms_file{list_of_terms_filename,std::ios::out};
        
        outWriter->write(data.terms_offered,&terms_offered_file);
        outWriter->write(data.prerequisites,&prerequisites_file);
        outWriter->write(data.list_of_terms,&list_of_terms_file);
        
        terms_offered_file.close();
        prerequisites_file.close();
        list_of_terms_file.close();

        return EXIT_SUCCESS;
}

void handle_term_dirs(const std::set<fs::directory_entry>& term_dirs,
                      quatalog_data_t& data) {
        for(const auto& term : term_dirs) {
                if(!fs::is_directory(term)) continue;
                handle_term(term,data);
        }

}

void handle_term(const fs::directory_entry& term_entry,
                 quatalog_data_t& quatalog_data) {
        const fs::path dir = term_entry.path();
        const auto& dirname = dir.string();
        const auto& term = dir.stem().string();
        const auto& courses_filename = dirname + "/courses.json";
        const auto& prereqs_filename = dirname + "/prerequisites.json";
        std::fstream courses_file{courses_filename,std::ios::in};
        std::fstream prereqs_file{prereqs_filename,std::ios::in};

        std::cerr << "Processing term " << term << "..." << std::endl;
        quatalog_data.list_of_terms["all_terms"].append(term);

        term_data_t term_data;
        courses_file >> term_data.courses;
        prereqs_file >> term_data.prerequisites;

        course_handler_t* course_handler;
        if(term.substr(4,2) == "05") {
                quatalog_data.list_of_terms["all_terms"].append(term+"02");
                quatalog_data.list_of_terms["all_terms"].append(term+"03");
                course_handler = handle_course_summer;
        } else {
                course_handler = handle_course;
        }
        for(const auto& prefix : term_data.courses) {
                handle_prefix(prefix,term,quatalog_data,term_data,course_handler);
        }

        courses_file.close();
        prereqs_file.close();
}

void handle_prefix(const Json::Value& prefix,
                   const std::string& term,
                   quatalog_data_t& quatalog_data,
                   const term_data_t& term_data,
                   course_handler_t course_handler) {
        for(const auto& course : prefix["courses"]) {
                course_handler(course,term,quatalog_data,term_data.prerequisites);
        }
}

void handle_course(const Json::Value& course,
                   const std::string& term,
                   quatalog_data_t& data,
                   const Json::Value& term_prereqs) {
        std::string course_id = course["id"].asString();
        auto& course_terms = data.terms_offered[course_id];
        const Json::Value& sections = course["sections"];
        handle_everything(sections,course,term,course_terms,data.prerequisites,term_prereqs);
}

void handle_course_summer(const Json::Value& course,
                          const std::string& term,
                          quatalog_data_t& data,
                          const Json::Value& term_prereqs) {
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
        const auto& course_id = course["id"].asString();
        bool subterm0 = false, subterm1 = false, subterm2 = false;
        for(const auto& section : course["sections"]) {
                const auto& timeslot = section["timeslots"][0];
                const auto& dateEnd = timeslot["dateEnd"].asString();
                const auto& dateStart = timeslot["dateStart"].asString();
                subterm = 0;
                if(dateStart.substr(0,2) != "05") {
                        subterm = 2;
                        subterm1 = true;
                } else if(dateEnd.substr(0,2) != "08") {
                        subterm = 1;
                        subterm2 = true;
                } else {
                        subterm0 = true;
                }
                sections[subterm].append(section);
        }

        auto& course_terms = data.terms_offered[course_id];
        
        if(subterm0) {
                handle_everything(sections[0],
                                  course,
                                  term,
                                  course_terms,
                                  data.prerequisites,
                                  term_prereqs);
                return;
        }
        if(subterm1) {
                handle_everything(sections[1],
                                  course,
                                  term+"02",
                                  course_terms,
                                  data.prerequisites,
                                  term_prereqs);
        }
        if(subterm2) {
                handle_everything(sections[2],
                                  course,
                                  term+"03",
                                  course_terms,
                                  data.prerequisites,
                                  term_prereqs);
        }
}

void handle_everything(const Json::Value& sections,
                       const Json::Value& course,
                       const std::string& term,
                       Json::Value& course_terms,
                       Json::Value& out_prereqs,
                       const Json::Value& term_prereqs) {
        Json::Value& course_term = course_terms[term];
        const auto& course_id = course["id"].asString();
        course_term["title"] = course["title"];
        handle_sections(sections,course_term);
        course_terms["latest_term"] = term;
        handle_attributes(sections[0],course_id,course_term,out_prereqs);
        handle_prereqs(sections[0],course_id,out_prereqs,term_prereqs);
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
        iterateOnDelimitedString(instructor_str,
                                 std::regex(", ?"),
                                 [&](const std::string& inst_str) {
                if(inst_str == "TBA") return;
                instructors.insert(inst_str);
        });
}

void handle_attributes(const Json::Value& section,
                       const std::string& course_id,
                       Json::Value& course_term,
                       Json::Value& out_prereqs) {
        // Makes the JSON list of attributes
        Json::Value& term_attributes = course_term["attributes"];
        Json::Value attributes = Json::arrayValue;
        term_attributes = Json::arrayValue;

        iterateOnDelimitedString(section["attribute"].asString(),
                                 std::regex(" and |, "),
                                 [&](const std::string& attr_str) {
                handle_attribute(attr_str,
                                 attributes,
                                 term_attributes);
        });

        if(!attributes.empty())
                out_prereqs[course_id]["attributes"] = attributes;
}

void handle_attribute(const std::string& attribute,
                      Json::Value& attributes,
                      Json::Value& term_attributes) {
        // COVID year screwed these attributes up; we will ignore them
        if(attribute != "Hybrid:Online/In-Person Course"
        && attribute != "Online Course"
        && attribute != "In-Person Course") {
                attributes.append(attribute);
                handle_term_attribute(attribute,term_attributes);
        }
}

void handle_term_attribute(const std::string& attribute,
                           Json::Value& attributes) {
        // These are the attributes we want to display in the
        // course years table
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

template<typename Functor>
void iterateOnDelimitedString(const std::string& str,
                              const std::regex& delim,
                              const Functor& callback) {
        // This mess is basically C++'s string split but not using
        // as much memory as an actual string split
        const auto end_itr = std::sregex_token_iterator();
        auto itr = std::sregex_token_iterator(str.begin(),str.end(),delim,-1);

        while(itr != end_itr && !itr->str().empty()) {
                callback(itr->str());
                itr++;
        }
}

void handle_prereqs(const Json::Value& section,
                    const std::string& course_id,
                    Json::Value& out_data,
                    const Json::Value& term_prereqs) {
        const auto& crn = section["crn"].asString();
        
        const auto& in_obj = term_prereqs[crn];

        const auto& corequisites = in_obj["corequisites"];
        const auto& prerequisites = in_obj["prerequisites"];
        const auto& cross_listings = in_obj["cross_list_courses"];
        
        out_data[course_id]["corequisites"] = corequisites;
        out_data[course_id]["prerequisites"] = prerequisites;
        out_data[course_id]["cross_listings"] = cross_listings;
}
