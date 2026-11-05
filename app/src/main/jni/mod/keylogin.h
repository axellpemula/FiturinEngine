#pragma once
#include <string>
#include <thread>
#include <fstream>
#include <chrono>
#include <iostream>
#include <curl/curl.h>
//Original src For free by @xvaluex : @FiturinEngine.
namespace EngineAuth {
    
    static char inputKey[128] = "";
    static bool isLoggedIn = false;
    static bool isChecking = false;
    static bool showLoginCard = true;
    static std::string statusMessage = "";
    static ImVec4 statusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    
    static std::string toastMsg = "";
    static float toastTimer = 0.0f;
    static ImVec4 toastColor = ImVec4(0.3f, 0.69f, 0.31f, 1.0f); 
    const std::string SAVE_FILE_PATH = "/sdcard/FiturinEngine_SavedKey.txt";
    const std::string HWID = "WEB-TEST-DEVICE-001"; 
    const std::string TARGET_GAME = "fiturin8bp"; 
    inline void ShowToast(const std::string& msg, bool success = true) {
        toastMsg = msg;
        toastTimer = 3.0f; 
        toastColor = success ? ImVec4(0.3f, 0.69f, 0.31f, 1.0f) : ImVec4(0.95f, 0.26f, 0.21f, 1.0f);
    }

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    inline void SaveKey(const std::string& key) {
        std::ofstream saveFile(SAVE_FILE_PATH);
        if (saveFile.is_open()) {
            saveFile << key;
            saveFile.close();
        }
    }

    inline void LoadSavedKey() {
        std::ifstream loadFile(SAVE_FILE_PATH);
        if (loadFile.is_open()) {
            std::string saved;
            std::getline(loadFile, saved);
            if (!saved.empty()) {
                strncpy(inputKey, saved.c_str(), sizeof(inputKey));
                loadFile.close();
                return;
            }
        }
    }

    inline std::string ExtractJsonString(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\":\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) {
            searchKey = "\"" + key + "\": \"";
            pos = json.find(searchKey);
        }
        if (pos != std::string::npos) {
            size_t start = pos + searchKey.length();
            size_t end = json.find("\"", start);
            if (end != std::string::npos) {
                return json.substr(start, end - start);
            }
        }
        return "";
    }

    inline void VerifyKeyAsync(std::string keyToTest, bool isAutoLogin) {
        isChecking = true;
        if (!isAutoLogin) {
            statusMessage = "Connecting to server...";
            statusColor = ImVec4(1.0f, 0.92f, 0.23f, 1.0f); 
        }

        std::thread([keyToTest, isAutoLogin]() {
            CURL* curl;
            CURLcode res;
            std::string readBuffer;

            curl = curl_easy_init();
            if (curl) {
                
                curl_easy_setopt(curl, CURLOPT_URL, "https://");
                
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "Accept: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                std::string jsonData = "{\"key\": \"" + keyToTest + "\", \"hwid\": \"" + HWID + "\", \"game\": \"" + TARGET_GAME + "\"}";
                
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 

                res = curl_easy_perform(curl);
                
                if (res == CURLE_OK) {
                    if (readBuffer.find("\"success\":true") != std::string::npos || readBuffer.find("\"success\": true") != std::string::npos) {
                        isLoggedIn = true;
                        showLoginCard = false;
                        SaveKey(keyToTest);
                        ShowToast("Successful", true);
                    } else {

                        std::string serverMsg = ExtractJsonString(readBuffer, "message");
                        if (serverMsg.empty()) serverMsg = "Invalid or expired key";

                        statusMessage = "ERROR: " + serverMsg;
                        statusColor = ImVec4(0.95f, 0.26f, 0.21f, 1.0f); // Red
                        ShowToast("Authentication Failed", false);
                        if (isAutoLogin) remove(SAVE_FILE_PATH.c_str());
                    }
                } else {
                    statusMessage = "ERROR: Failed to connect to server";
                    statusColor = ImVec4(0.95f, 0.26f, 0.21f, 1.0f); // Red
                    ShowToast("Network Error", false);
                }
                
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
            isChecking = false;
        }).detach();
    }

    // Main Draw Call
    inline void DrawLoginCard() {
        static bool init = false;
        if (!init) {
            LoadSavedKey();
            if (strlen(inputKey) > 0) {
                VerifyKeyAsync(inputKey, true);
            }
            init = true;
        }

        ImGuiIO& io = ImGui::GetIO();

        // 1. Draw Toast Notification globally
        if (toastTimer > 0.0f) {
            toastTimer -= io.DeltaTime;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 50.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(51, 51, 51, 255)); // Dark Grey toast bg
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 25.0f);
            
            ImGui::Begin("ToastWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
            ImGui::TextColored(toastColor, "%s", toastMsg.c_str());
            ImGui::End();
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        if (!showLoginCard) return;

        // 2. Draw Login Card
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(360, 0)); // Fixed width to match design
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(30, 30, 30, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // Remove padding for custom header
        
        ImGui::Begin("AuthCard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

        // --- Custom Header (Green) ---
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        draw_list->AddRectFilled(pos, ImVec2(pos.x + 360, pos.y + 70), IM_COL32(46, 125, 50, 255), 8.0f, ImDrawFlags_RoundCornersTop); 
        
        ImGui::SetCursorPos(ImVec2(20, 15));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Fiturin Engine");
        ImGui::SetCursorPos(ImVec2(20, 40));
        // Updated Subtitle
        ImGui::TextColored(ImVec4(1, 1, 1, 0.9f), "Target Game : fiturin8bp"); 
        
        // Badge text drawing
        draw_list->AddRectFilled(ImVec2(pos.x + 290, pos.y + 15), ImVec2(pos.x + 360, pos.y + 35), IM_COL32(255, 255, 255, 51), 4.0f, ImDrawFlags_RoundCornersLeft);
        draw_list->AddText(ImVec2(pos.x + 298, pos.y + 18), IM_COL32(255, 255, 255, 255), "FITURIN");
        
        ImGui::Dummy(ImVec2(0, 70)); // Space below header

        // --- Body Content ---
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
        ImGui::BeginChild("BodyContent", ImVec2(0, 230), false, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.88f, 1.0f), "Enter your private key to authenticate");
        ImGui::Spacing();

        // Input and Paste Button Group
        ImGui::PushItemWidth(240);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(17, 17, 17, 255));
        // Updated Placeholder
        ImGui::InputTextWithHint("##KeyInput", "Enter Key Here", inputKey, IM_ARRAYSIZE(inputKey), ImGuiInputTextFlags_Password); 
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(51, 51, 51, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(68, 68, 68, 255));
        if (ImGui::Button("PASTE", ImVec2(70, 0))) {
            const char* clipText = ImGui::GetClipboardText();
            if (clipText != nullptr && strlen(clipText) > 0) {
                strncpy(inputKey, clipText, sizeof(inputKey) - 1);
                ShowToast("Key Pasted Successful", true);
            } else {
                ShowToast("No Key Copied.. First Copy and Try Again", false);
            }
        }
        ImGui::PopStyleColor(2);

        // Status Message
        if (!statusMessage.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(statusColor, "%s", statusMessage.c_str());
        } else {
            ImGui::Dummy(ImVec2(0, 18)); // Placeholder to prevent jumping UI
        }
        ImGui::Spacing();

        // Auth Button
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(46, 125, 50, 255)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(27, 94, 32, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(20, 64, 22, 255));
        
        if (isChecking) {
            ImGui::BeginDisabled(true); 
            ImGui::Button("AUTHENTICATING...", ImVec2(320, 35));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button("AUTHENTICATION", ImVec2(320, 35))) {
                if (strlen(inputKey) > 0) {
                    VerifyKeyAsync(inputKey, false);
                } else {
                    statusMessage = "ERROR: Please enter a key";
                    statusColor = ImVec4(0.95f, 0.26f, 0.21f, 1.0f);
                }
            }
        }
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing(); ImGui::Spacing();

        // Footer Text and Links
        ImGui::TextColored(ImVec4(0.66f, 0.66f, 0.66f, 1.0f), "Want To Become Reseller? ");
        ImGui::SameLine(0, 0);
        ImGui::TextColored(ImVec4(0.3f, 0.69f, 0.31f, 1.0f), "Contact Owner");
        if (ImGui::IsItemClicked()) {
            system("am start -a android.intent.action.VIEW -d https://t.me/axell_08 > /dev/null 2>&1");
        }
        
        ImGui::Spacing();
        
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(42, 42, 42, 255));
        // Updated Community Button
        if (ImGui::Button("Join Telegram: @FiturinEngine", ImVec2(320, 30))) { 
            system("am start -a android.intent.action.VIEW -d https://t.me/FiturinEngine > /dev/null 2>&1");
        }
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleVar(); 
        
        ImGui::End(); 
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
}
