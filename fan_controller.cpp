#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <map>
#include <algorithm>

// For convenience
using json = nlohmann::json;

// Default configuration values
const std::string DEFAULT_FAN_IP = "192.168.7.193";
const double DEFAULT_TEMPERATURE_THRESHOLD = 75.0;
const int DEFAULT_CHECK_INTERVAL = 60;
const std::string DEFAULT_TEMP_SOURCE = "inside";
const std::string DEFAULT_FAN_NAME = "Airscape Fan";

// Callback function for curl to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc &e) {
        // Handle memory problem
        return 0;
    }
}
// sanatize server response 
static std::string sanitizeForJson(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    size_t i = 0, n = in.size();
    while (i < n) {
        unsigned char c = in[i];

        if (c < 0x80) {                              // ASCII
            // drop control chars JSON won't accept unescaped; keep \t \n \r
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
                out += '?';
            else
                out += static_cast<char>(c);
            ++i;
            continue;
        }

        size_t len;
        if      ((c >> 5) == 0x6)  len = 2;
        else if ((c >> 4) == 0xE)  len = 3;
        else if ((c >> 3) == 0x1E) len = 4;
        else { out += '?'; ++i; continue; }          // bad lead byte

        if (i + len > n) { out += '?'; ++i; continue; }
        bool ok = true;
        for (size_t j = 1; j < len; ++j)
            if ((static_cast<unsigned char>(in[i+j]) >> 6) != 0x2) { ok = false; break; }

        if (ok) { out.append(in, i, len); i += len; }
        else    { out += '?'; ++i; }
    }
    return out;
}
// 
static std::string dropServerResponse(const std::string& in) {
    const std::string key = "\"server_response\"";
    auto k = in.find(key);
    if (k == std::string::npos) return in;

    // find the opening quote of the value
    auto colon = in.find(':', k + key.size());
    auto vq = in.find('"', colon + 1);
    if (vq == std::string::npos) return in;

    // value runs until the LAST quote before the next key/line.
    // device terminates it with: "<binary>",\n  "dip_switches"
    auto next = in.find("\"dip_switches\"", vq);
    if (next == std::string::npos) return in;

    // splice out "server_response": "...", keeping JSON valid
    std::string out = in.substr(0, k);
    out += in.substr(next);     // resume at the next key
    return out;
}
// Function to get the current fan status
json getFanStatus(const std::string& fanIp) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    json responseJson;
    
    curl = curl_easy_init();
    if(curl) {
        // Using the correct endpoint from the API documentation
        std::string url = "http://" + fanIp + "/status.json.cgi";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if(res != CURLE_OK) {
            std::cerr << "Failed to get fan status: " << curl_easy_strerror(res) << std::endl;
        } else {
            try {
                auto something = dropServerResponse(readBuffer);
                std::cout << something << std::endl;
                responseJson = json::parse(something);
                
                std::cout << "Fan status retrieved successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
                //std::cerr << "Raw response: " << responseJson << std::endl;
            }
        }
    }
    
    return responseJson;
}



// Function to turn the fan off
bool turnFanOff(const std::string& fanIp) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    bool success = false;
    
    curl = curl_easy_init();
    if(curl) {
        // This endpoint is correct according to the API documentation
        std::string url = "http://" + fanIp + "/fanspd.cgi?dir=4";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if(res != CURLE_OK) {
            std::cerr << "Failed to turn fan off: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "Fan off command sent successfully" << std::endl;
            success = true;
        }
    }
    
    return success;
}

// Function to get current temperature from the fan's built-in sensors
double getCurrentTemperature(const json& status, const std::string& tempSource) {
    double temperature = 0.0;
    bool tempFound = false;
    
    // Get temperature based on configured source
    if (!status.empty()) {
        try {
            // Using the exact field names from the JSON response
            if (tempSource == "inside" && status.contains("inside")) {
                temperature = status["inside"].get<int>();
                tempFound = true;
                std::cout << "Inside temperature: " << temperature << "°F" << std::endl;
            } 
            else if (tempSource == "attic" && status.contains("attic")) {
                temperature = status["attic"].get<int>();
                tempFound = true;
                std::cout << "Attic temperature: " << temperature << "°F" << std::endl;
            } 
            else if (tempSource == "oa" && status.contains("oa")) {
                temperature = status["oa"].get<int>();
                tempFound = true;
                std::cout << "Outside air temperature: " << temperature << "°F" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing temperature: " << e.what() << std::endl;
        }
    }
    
    if (!tempFound) {
        std::cerr << "Temperature data not found in status response. Using fallback temperature." << std::endl;
        // Fallback to a mock temperature if we can't get it from the fan
        static double mockTemp = 80.0;
        mockTemp -= 0.5;
        std::cout << "Using mock temperature: " << mockTemp << "°F" << std::endl;
        return mockTemp;
    }
    
    return temperature;
}

// Function to display fan information
void displayFanInfo(const json& status, const std::string& fanName) {
    if (!status.empty()) {
        try {
            std::cout << "==== " << fanName << " Information ====" << std::endl;
            if (status.contains("model")) {
                std::cout << "Model: " << status["model"].get<std::string>() << std::endl;
            }
            if (status.contains("softver")) {
                std::cout << "Software version: " << status["softver"].get<std::string>() << std::endl;
            }
            if (status.contains("ipaddr")) {
                std::cout << "IP address: " << status["ipaddr"].get<std::string>() << std::endl;
            }
            if (status.contains("macaddr")) {
                std::cout << "MAC address: " << status["macaddr"].get<std::string>() << std::endl;
            }
            
            // Display all available temperatures
            if (status.contains("inside")) {
                std::cout << "Inside temperature: " << status["inside"].get<int>() << "°F" << std::endl;
            }
            if (status.contains("attic")) {
                std::cout << "Attic temperature: " << status["attic"].get<int>() << "°F" << std::endl;
            }
            if (status.contains("oa")) {
                std::cout << "Outside air temperature: " << status["oa"].get<int>() << "°F" << std::endl;
            }
            
            // Display current fan speed and power
            if (status.contains("fanspd")) {
                std::cout << "Fan speed: " << status["fanspd"].get<int>() << std::endl;
            }
            if (status.contains("power")) {
                std::cout << "Power consumption: " << status["power"].get<int>() << " watts" << std::endl;
            }
            if (status.contains("cfm")) {
                std::cout << "Airflow: " << status["cfm"].get<int>() << " CFM" << std::endl;
            }
            
            std::cout << "==========================" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error displaying fan information: " << e.what() << std::endl;
        }
    }
}

// Function to display usage information
void printUsage(const std::string& programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --ip=ADDRESS      IP address of the Airscape fan (default: " << DEFAULT_FAN_IP << ")" << std::endl;
    std::cout << "  --temp=VALUE      Temperature threshold in °F (default: " << DEFAULT_TEMPERATURE_THRESHOLD << ")" << std::endl;
    std::cout << "  --source=SOURCE   Temperature source to use (inside, attic, oa) (default: " << DEFAULT_TEMP_SOURCE << ")" << std::endl;
    std::cout << "  --name=NAME       Name for the fan (default: " << DEFAULT_FAN_NAME << ")" << std::endl;
    std::cout << "  --interval=SECS   Check interval in seconds (default: " << DEFAULT_CHECK_INTERVAL << ")" << std::endl;
    std::cout << "  --help            Display this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " --ip=192.168.1.100 --temp=72 --source=inside" << std::endl;
    std::cout << "  " << programName << " --ip=192.168.1.100 --name=\"Attic Fan\" --temp=75" << std::endl;
}

// Function to parse command-line arguments
std::map<std::string, std::string> parseArgs(int argc, char* argv[]) {
    std::map<std::string, std::string> args;
    
    // Set default values
    args["ip"] = DEFAULT_FAN_IP;
    args["temp"] = std::to_string(DEFAULT_TEMPERATURE_THRESHOLD);
    args["source"] = DEFAULT_TEMP_SOURCE;
    args["name"] = DEFAULT_FAN_NAME;
    args["interval"] = std::to_string(DEFAULT_CHECK_INTERVAL);
    
    // Parse named arguments (--param=value format)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Check for help flag
        if (arg == "--help" || arg == "-h") {
            args["help"] = "true";
            return args;
        }
        
        // Check for named parameters
        if (arg.substr(0, 2) == "--") {
            size_t equalsPos = arg.find('=');
            if (equalsPos != std::string::npos) {
                std::string key = arg.substr(2, equalsPos - 2);
                std::string value = arg.substr(equalsPos + 1);
                args[key] = value;
            }
        }
        // Handle positional arguments as a fallback
        else if (i == 1 && arg.find('=') == std::string::npos) {
            args["ip"] = arg;  // First positional arg is IP
        }
        else if (i == 2 && arg.find('=') == std::string::npos) {
            args["temp"] = arg;  // Second positional arg is temperature
        }
        else if (i == 3 && arg.find('=') == std::string::npos) {
            args["source"] = arg;  // Third positional arg is source
        }
        else if (i == 4 && arg.find('=') == std::string::npos) {
            args["name"] = arg;  // Fourth positional arg is name
        }
    }
    
    return args;
}

// Validate temperature source
bool isValidTempSource(const std::string& source) {
    return (source == "inside" || source == "attic" || source == "oa");
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    std::map<std::string, std::string> args = parseArgs(argc, argv);
    
    // Check if help was requested
    if (args.find("help") != args.end()) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Extract and validate settings
    std::string fanIp = args["ip"];
    std::string fanName = args["name"];
    std::string tempSource = args["source"];
    
    // Validate temperature source
    if (!isValidTempSource(tempSource)) {
        std::cerr << "Error: Invalid temperature source '" << tempSource << "'" << std::endl;
        std::cerr << "Valid options are: inside, attic, oa" << std::endl;
        return 1;
    }
    
    // Parse numeric values with error handling
    double temperatureThreshold;
    int checkInterval;
    
    try {
        temperatureThreshold = std::stod(args["temp"]);
        checkInterval = std::stoi(args["interval"]);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing numeric parameters: " << e.what() << std::endl;
        return 1;
    }
    
    // Display configuration
    std::cout << fanName << " Temperature Controller" << std::endl;
    std::cout << "Fan IP: " << fanIp << std::endl;
    std::cout << "Temperature threshold: " << temperatureThreshold << "°F" << std::endl;
    std::cout << "Temperature source: " << tempSource << std::endl;
    std::cout << "Checking every " << checkInterval << " seconds" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_ALL);
    
    bool fanTurnedOff = false;
    bool firstRun = true;
    
    // Main control loop
    while(true) {
        // Get fan status - this includes temperature data
        json status = getFanStatus(fanIp);
        
        // Display fan information on first run
        if (firstRun && !status.empty()) {
            displayFanInfo(status, fanName);
            firstRun = false;
        }
        
        // Get current temperature from the fan's sensors
        double currentTemp = getCurrentTemperature(status, tempSource);
        
        // Check if fan is running
        bool fanIsRunning = false;
        if (!status.empty() && status.contains("fanspd")) {
            try {
                int fanSpeed = status["fanspd"].get<int>();
                fanIsRunning = (fanSpeed > 0);
                std::cout << "Current fan speed: " << fanSpeed << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing fan speed: " << e.what() << std::endl;
            }
        }
        
        if (currentTemp <= temperatureThreshold && fanIsRunning && !fanTurnedOff) {
            std::cout << "Temperature below threshold. Turning fan off." << std::endl;
            if (turnFanOff(fanIp)) {
                fanTurnedOff = true;
            }
        } else if (currentTemp > temperatureThreshold && fanTurnedOff) {
            std::cout << "Temperature above threshold. Fan control returned to manual." << std::endl;
            fanTurnedOff = false;
        }
        
        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
    }
    
    curl_global_cleanup();
    return 0;
}