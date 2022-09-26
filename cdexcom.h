#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <regex>
#include <curl/curl.h>
#include "json.h"
using json = nlohmann::ordered_json;

#define DEXCOM_BASE_URL "https://share2.dexcom.com/ShareWebServices/Services"
#define DEXCOM_BASE_URL_OUS "https://shareous1.dexcom.com/ShareWebServices/Services"
#define DEXCOM_LOGIN_ID_ENDPOINT "General/LoginPublisherAccountById"
#define DEXCOM_AUTHENTICATE_ENDPOINT "General/AuthenticatePublisherAccount"
#define DEXCOM_VERIFY_SERIAL_NUMBER_ENDPOINT "Publisher/CheckMonitoredReceiverAssignmentStatus"
#define DEXCOM_GLUCOSE_READINGS_ENDPOINT "Publisher/ReadPublisherLatestGlucoseValues"
#define DEXCOM_APPLICATION_ID "d89443d2-327c-4a6f-89e5-496bbb0317db"
#define DEFAULT_SESSION_ID "00000000-0000-0000-0000-000000000000"
#define MMOL_L_CONVERTION_FACTOR 0.0555f
std::unordered_map <std::string, std::string> DEXCOM_TREND = {
    {"DoubleUp","↑↑"},
    {"SingleUp","↑"},
    {"FortyFiveUp","↗"},
    {"Flat","→"},
    {"FortyFiveDown","↘"},
    {"SingleDown","↓"},
    {"DoubleDown","↓↓"},
    {"NotComputable","?"},
    {"RateOutOfRange","-"}
};


static int writer(char *data, size_t size, size_t nmemb, std::string *buffer_in)
{
    if (buffer_in != NULL)  
    {
        buffer_in->append(data, size * nmemb);

        return size * nmemb;  
    }

    return 0;
}   

float round(float var)
{
    float value = (int)(var * 10 + .5);
    return (float)value / 10;
}

std::string DownloadedResponse;

struct GlucoseReading
{
    int value;
    int mg_dl;
    float mmol_l;
    std::string trend;
    std::string trend_arrow;

    GlucoseReading(json glucose_readings);
    ~GlucoseReading();
};

GlucoseReading::GlucoseReading(json glucose_readings)
{
    value = glucose_readings["Value"];
    mg_dl = value;
    mmol_l = round(mg_dl * MMOL_L_CONVERTION_FACTOR);
    trend = glucose_readings["Trend"];
    trend_arrow = DEXCOM_TREND[trend];
}

GlucoseReading::~GlucoseReading(){}

struct Dexcom
{
private:
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
public:
    std::string base_url;
    std::string username;
    std::string password;
    std::string session_id;
    std::string account_id;

    Dexcom(std::string _username, std::string _password, bool ous);
    ~Dexcom();
    json request(std::string endpoint, json postfields);
    void create_session();
    std::vector<GlucoseReading> get_glucose_readings(int minutes, int max_count);
    GlucoseReading get_latest_glucose_reading();
    GlucoseReading get_current_glucose_reading();
};

Dexcom::Dexcom(std::string _username, std::string _password, bool ous)
{
    curl = curl_easy_init();

    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &DownloadedResponse);

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);


    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

    base_url = ous ? DEXCOM_BASE_URL_OUS : DEXCOM_BASE_URL;
    username = _username;
    password = _password;
    session_id = "";
    account_id = "";
    create_session();
}

Dexcom::~Dexcom()
{   
    curl_slist_free_all(headers); 
    curl_easy_cleanup(curl);
}

json Dexcom::request(std::string endpoint, json postfields)
{
    json response;
    DownloadedResponse = "";
    if (curl)
    {
        std::string url = base_url + '/' + endpoint;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        std::string fields = postfields.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields.c_str());
        CURLcode res = curl_easy_perform(curl);
        response = json::parse(DownloadedResponse);
    }
    return response;
}   

void Dexcom::create_session()
{
    json get_account_id = {
        {"accountName", username},
        {"password", password},
        {"applicationId", DEXCOM_APPLICATION_ID}
    };
    account_id = request(DEXCOM_AUTHENTICATE_ENDPOINT, get_account_id).dump();
    account_id.erase(std::remove(account_id.begin(), account_id.end(), '\"'), account_id.end());

    json get_session_id = {
        {"accountId", account_id},
        {"password", password},
        {"applicationId", DEXCOM_APPLICATION_ID}
    };

    session_id = request(DEXCOM_LOGIN_ID_ENDPOINT, get_session_id).dump();
    session_id.erase(std::remove(session_id.begin(), session_id.end(), '\"'), session_id.end());
    
}

std::vector<GlucoseReading> Dexcom::get_glucose_readings(int minutes, int max_count)
{
    std::vector<GlucoseReading> glucose_readings;
    json params = {
        {"sessionId", session_id},
        {"minutes", minutes},
        {"maxCount", max_count}
    };
    json glucose_readings_json = request(DEXCOM_GLUCOSE_READINGS_ENDPOINT, params);
    for (auto& reading : glucose_readings_json)
    {
        GlucoseReading gr(reading);
        glucose_readings.push_back(gr);
    }
   return glucose_readings;
}

GlucoseReading Dexcom::get_latest_glucose_reading()
{
    std::vector<GlucoseReading> glucose_readings = get_glucose_readings(1440, 1);
    return glucose_readings[0];
}

GlucoseReading Dexcom::get_current_glucose_reading()
{
    std::vector<GlucoseReading> glucose_readings = get_glucose_readings(10, 1);
    return glucose_readings[0];
}