#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <windows.h>
#include <mmsystem.h>
#include <conio.h>
#include <Xinput.h>
#include <fstream>
#include <opencv2/opencv.hpp>
#include "tesseract/baseapi.h"
#include <mutex>
#include <limits>

// Automatically connects Windows multimedia libraries and controller drivers.
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Xinput.lib")

using namespace std;

tesseract::TessBaseAPI* g_OcrApi = nullptr;
std::atomic<bool> g_IsRunning(true);

//My resolution
int g_TargetWidth = 1920;
int g_TargetHeight = 1200;

double g_Scale = 1.0;
int g_CurrentVolume = 50;
std::atomic<bool> g_CommandMode(false);

std::mutex g_ListMutex;

bool g_DebugMode = false;

void RunFullSetupWizard();

int g_BindKeyUp = 'W';
int g_BindKeyDown = 'S';
int g_PotatoScanDelayMs = 0;
bool g_MutePulse = false;


bool g_OnlyBlueMissions = true;
bool g_ScanMissionName = true;
bool g_ScanMissionReward = true;


long long g_MinCreditReward = 0;

// Blacklist/whitelist container array
std::vector<std::string> g_WhiteList;
std::vector<std::string> g_BlackList;

std::vector<cv::Mat> g_WasteImgFingerprints;
std::vector<cv::Mat> g_WasteGoldFingerprints;
std::vector<cv::Mat> g_FullScanWhiteFingerprints;
std::vector<cv::Mat> g_FullScanGoldFingerprints;

// memory bank
const std::vector<std::string> PKG_WMM_WHITE = { "GOLD", "GCLD", "G0LD", "G0CD", "SILVER", "S1LVER", "SLVER", "BERTRANDITE", "8ERTRANDITE", "BERTRAND1TE", "INDITE", "IND1TE", "INDETE", "IND1T" };
const std::vector<std::string> PKG_WMM_BLACK = { "ACQUIRE", "SOURCE", "RETURN", "MEAT", "BROMELLITE", "INDIUM", "INDUSTRY", "SUPPLY", "BRING", "DONATE" };
const std::vector<std::string> PKG_RARE_MATS = {
    "PHARMACEUTICAL", "PHARMACEUT1CAL", "PHARMACEUTLCAL", "MILITARY", "MlLlTARY", "MILITARY", "EXQUISITE", "EXQU1S1TE", "EXQUlSlTE", "BIOTECH", "BlOTECH", "BI0TECH", "IMPERIAL", "lMPERlAL", "IMPERlAL",
    "CORE", "C0RE", "COEE", "PROTO", "PR0T0", "PE0T0", "CHEMICAL", "CHEM1CAL", "CHEMLCAL", "MANUFACTURED", "MANUFACTUEED", "MANUFACTURE0", "DATAMINE", "DATAM1NE", "DATAMlNE", "EMBEDDED", "EMBED0E0", "EMBE00E0",
    "ADAPTIVE", "ADAPT1VE", "ADAPTlVE", "SMEAR", "SMEAE", "5MEAR", "OPINION", "0P1N10N", "OPlNlON", "FINANCIAL", "FINANC1AL", "FINANCLAL", "SETTLEMENT", "SETTLENENT", "5ETTLEMENT", "MANUFACTURING", "MANUFACTUR1NG", "MANUFACTURlNG",
    "TITANIUM", "T1TAN1UM", "TlTAlNlUM", "GRAPHENE", "GEAPHENE", "GRAPHENE", "WEAPON", "WEAP0N", "WEAPDN", "SUIT", "5UIT", "SUlT"
};
const std::vector<std::string> PKG_RARE_GOODS = {
    "BROMELLITE", "BR0MELL1TE", "BROMELllTE", "PAINITE", "PA1N1TE", "PAlNlTE", "PLATINUM", "PLAT1NUM", "PLATlNUM", "ALEXANDRITE", "ALEXANDR1TE", "ALEXANDRlTE", "OPAL", "0PAL", "0PAl",
    "DIAMOND", "D1AM0ND", "DlAMOND", "VOID", "V01D", "VOlD", "MUSGRAVITE", "MUSGRAV1TE", "MUSGRAVlTE", "RHODPLUMSIT", "RH0DPLUMS1T", "RHODPLUMSlT", "MONAZITE", "M0NAZ1TE", "MONAZlTE", "SERENDIBITE", "SEREND1B1TE", "SERENDlBlTlE",
    "TRITIUM", "TR1T1UM", "TRlTlUM", "GOLD", "G0LD", "GOCD", "SILVER", "S1LVER", "SlLVER", "BERTRANDITE", "BERTRAND1TE", "BERTRANDlTE"
};

// Config & Hardware IO------------------------------------------------------------------------

void SaveConfigToDisk() {
    std::ofstream outFile(".\\config.txt");
    if (!outFile) return;

    outFile << "[WHITE]\n";
    for (const auto& w : g_WhiteList) outFile << w << "\n";

    outFile << "[BLACK]\n";
    for (const auto& b : g_BlackList) outFile << b << "\n";

    outFile << "[REWARD]\n" << g_MinCreditReward << "\n";

    outFile.close();
}

void LoadConfigFromDisk() {
    std::ifstream inFile(".\\config.txt");
    if (!inFile) {
        g_WhiteList = PKG_WMM_WHITE;
        g_BlackList = PKG_WMM_BLACK;
        g_MinCreditReward = 0;
        SaveConfigToDisk();
        return;
    }

    g_WhiteList.clear();
    g_BlackList.clear();
    g_MinCreditReward = 0;
    std::string line, mode = "";

    while (std::getline(inFile, line)) {
        if (line.empty()) continue;
        if (line == "[WHITE]") { mode = "W"; continue; }
        if (line == "[BLACK]") { mode = "B"; continue; }
        if (line == "[REWARD]") { mode = "R"; continue; }

        if (mode == "W") g_WhiteList.push_back(line);
        if (mode == "B") g_BlackList.push_back(line);
        if (mode == "R") {
            try { g_MinCreditReward = std::stoll(line); }
            catch (...) { g_MinCreditReward = 0; }
        }
    }
    inFile.close();
}

// sound-----------------------------------------------------------------------------

void ApplyHardwareVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    // Convert 0-100 percentage to Windows 16-bit volume format (0x0000 - 0xFFFF)
    WORD volumeWord = static_cast<WORD>((volume / 100.0) * 0xFFFF);
    DWORD waveVolume = (DWORD)volumeWord | ((DWORD)volumeWord << 16);

    // Modify hardware stream volume for current thread session explicitly
    waveOutSetVolume(NULL, waveVolume);
    std::cout << "[VOLUME CHANGE] Radar output volume adjusted to: " << volume << "%\n";
}

void PlayTargetDetectedAlertAsync() {
    static std::atomic<bool> isPlaying(false);
    if (isPlaying) return;

    std::thread([]() {
        isPlaying = true;

        PlaySoundW(L".\\airhorn.wav", NULL, SND_FILENAME | SND_NODEFAULT);

        isPlaying = false;
        }).detach();
}

// Blacklist and Whitelist----------------------------------------------------------------------------

bool EvaluateMission(string text) {

    std::lock_guard<std::mutex> lock(g_ListMutex);

    transform(text.begin(), text.end(), text.begin(), ::toupper);

    // blacklist
    for (const auto& blackWord : g_BlackList) {
        if (text.find(blackWord) != string::npos) {
            return false;
        }
    }

    if (g_MinCreditReward > 0) {
        std::string pureDigits = "";
        for (char c : text) {
            if (c >= '0' && c <= '9') {
                pureDigits += c;
            }
        }

        if (!pureDigits.empty()) {
            try {
                long long detectedCredits = std::stoll(pureDigits);
                if (detectedCredits >= g_MinCreditReward) {
                    return true;
                }
            }
            catch (...) {
            }
        }
    }

    // Whitelist
    for (const auto& whiteWord : g_WhiteList) {
        if (text.find(whiteWord) != string::npos) {
            return true;
        }
    }
    return false;
}

// Image preprocessing----------------------------------------------------------------------

cv::Mat PreprocessImageForOCR(const cv::Mat& src) {
    if (src.empty()) return src;
    cv::Mat grayRow, binarizedRow, resizedRow;

    cv::cvtColor(src, grayRow, cv::COLOR_BGR2GRAY);
    // Otsu binarization
    cv::threshold(grayRow, binarizedRow, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    // 2x magnification
    cv::resize(binarizedRow, resizedRow, cv::Size(), 2.0, 2.0, cv::INTER_NEAREST);

    return resizedRow;
}

// OCR--------------------------------------------------------------------------------

bool EvaluateMissionOCR(const cv::Mat& processedMat) {
    if (processedMat.empty() || !g_OcrApi) return false;

    cv::Mat continuousMat = processedMat.clone();

    // OCR
    g_OcrApi->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_LINE);
    g_OcrApi->SetImage(continuousMat.data, continuousMat.cols, continuousMat.rows, 1, continuousMat.cols);

    char* ocrResult = g_OcrApi->GetUTF8Text();
    if (!ocrResult) return false;

    std::string text(ocrResult);
    delete[] ocrResult; // No memory leaks

    // 全強制轉大寫進行名單庫物理匹配
    std::transform(text.begin(), text.end(), text.begin(), ::toupper);

    // Credits
    if (g_MinCreditReward > 0) {
        std::string numStr = "";
        for (char c : text) {
            if (c >= '0' && c <= '9') numStr += c;
        }
        if (!numStr.empty()) {
            long long reward = std::stoll(numStr);
            if (reward >= g_MinCreditReward) return true;
        }
    }

    // Blacklist and whitelist
    for (const auto& word : g_WhiteList) {
        if (text.find(word) != std::string::npos) {

            for (const auto& black : g_BlackList) {
                if (text.find(black) != std::string::npos) return false;
            }
            return true;
        }
    }
    return false;
}

// BRAIN=============================================================================================================

bool ScanMissionsByExactGeometry(const cv::Mat& fullFrame) {
    if (fullFrame.empty()) return false;

    // An alarm will not be triggered repeatedly at the same height within 1.5 seconds.
    static std::vector<std::pair<int, std::chrono::steady_clock::time_point>> reportedMissions;
    auto now = std::chrono::steady_clock::now();
    reportedMissions.erase(std::remove_if(reportedMissions.begin(), reportedMissions.end(), [now](const auto& item) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - item.second).count() > 1500;
        }), reportedMissions.end());

    int channelX = static_cast<int>(116 * g_Scale);
    int channelW = static_cast<int>(1176 * g_Scale);
    int channelY = static_cast<int>(502 * g_Scale);
    int channelH = static_cast<int>(472 * g_Scale);

    if (channelX + channelW > fullFrame.cols || channelY + channelH > fullFrame.rows || channelX < 0 || channelY < 0) return false;

    cv::Rect activeChannelRect(channelX, channelY, channelW, channelH);
    cv::Mat activeChannelFrame = fullFrame(activeChannelRect).clone();

    cv::Mat hsvChannel, grayChannel;
    cv::cvtColor(activeChannelFrame, hsvChannel, cv::COLOR_BGR2HSV);
    cv::cvtColor(activeChannelFrame, grayChannel, cv::COLOR_BGR2GRAY);

    // Color mask
    cv::Mat globalGoldMask, globalWhiteMask;
    cv::inRange(hsvChannel, cv::Scalar(18, 170, 150), cv::Scalar(28, 255, 255), globalGoldMask);
    cv::threshold(grayChannel, globalWhiteMask, 180, 255, cv::THRESH_BINARY);

    // CCTV---------------------------------------------------------------------------
    cv::Mat canvasBillboard = activeChannelFrame.clone();
    if (g_DebugMode) {
        cv::imshow("[Debug 1] Global Billboard", canvasBillboard);
    }

    if (g_DebugMode) {
        if (g_ScanMissionName) {
            cv::imshow("[Debug 3] Color Mask", globalWhiteMask);
        }
        else {
            cv::imshow("[Debug 3] Color Mask", globalGoldMask);
        }
    }
    bool anyTargetFound = false;

    //Only accept blue quests---------------------------------------------------------------------------------------

    if (g_OnlyBlueMissions) {
        cv::Mat blueMask;
        cv::inRange(hsvChannel, cv::Scalar(90, 100, 100), cv::Scalar(140, 255, 255), blueMask);

        cv::Mat labels, stats, centroids;
        int numComponents = cv::connectedComponentsWithStats(blueMask, labels, stats, centroids);

        for (int i = 1; i < numComponents; i++) {
            int compW = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int compH = stats.at<int>(i, cv::CC_STAT_HEIGHT);
            int area = stats.at<int>(i, cv::CC_STAT_AREA);

            if (compW >= (10 * g_Scale) && compW <= (60 * g_Scale) && compH >= (10 * g_Scale) && compH <= (60 * g_Scale) && area >= (30 * g_Scale * g_Scale) && stats.at<int>(i, cv::CC_STAT_LEFT) <= (30 * g_Scale)) {
                int blueY = stats.at<int>(i, cv::CC_STAT_TOP) + (compH / 2);
                int absoluteY = blueY + channelY;

                bool alreadyReported = false;
                for (const auto& item : reportedMissions) {
                    if (std::abs(item.first - absoluteY) < (25 * g_Scale)) { alreadyReported = true; break; }
                }
                if (alreadyReported) continue;

                // White text
                if (g_ScanMissionName) {
                    int textX = static_cast<int>(60 * g_Scale);
                    int origY = static_cast<int>(blueY - (57 * g_Scale));
                    int textY = std::max(0, origY);
                    int textW = static_cast<int>(700 * g_Scale);
                    int textH = static_cast<int>(33 * g_Scale);

                    if (textX + textW > activeChannelFrame.cols) textW = activeChannelFrame.cols - textX;
                    if (textY + textH > activeChannelFrame.rows) textH = activeChannelFrame.rows - textY;

                    if (textX >= 0 && textW > 0 && textH > 0) {
                        cv::Rect whiteRect(textX, textY, textW, textH);

                        cv::Mat colorTarget = activeChannelFrame(whiteRect).clone();

                        if (g_DebugMode) {
                            cv::imshow("[Debug 2] Binarized Target", colorTarget);
                            cv::waitKey(1);
                        }

                        cv::Mat binWhite;
                        cv::bitwise_not(globalWhiteMask(whiteRect), binWhite);
                        cv::resize(binWhite, binWhite, cv::Size(), 2.0, 2.0, cv::INTER_NEAREST);

                        int totalPixels = binWhite.cols * binWhite.rows;
                        int textPixels = totalPixels - cv::countNonZero(binWhite);
                        if (textPixels < (150 * g_Scale)) continue;
                        if (textPixels > (totalPixels * 0.80)) continue;

                        // Image feature fingerprint------------------------------------------------------------------

                        cv::Mat fingerprint;
                        cv::resize(binWhite, fingerprint, cv::Size(128, 8), 0, 0, cv::INTER_NEAREST);

                        bool isDuplicateWaste = false;
                        for (const auto& oldFinger : g_WasteImgFingerprints) {
                            cv::Mat diff;
                            cv::absdiff(fingerprint, oldFinger, diff);
                            double errorRatio = static_cast<double>(cv::countNonZero(diff)) / (128 * 8);

                            if (errorRatio < 0.03) {
                                isDuplicateWaste = true;
                                break;
                            }
                        }

                        // security guard-----------------------------------------------------------------------------
                        if (isDuplicateWaste) {
                            continue;
                        }

                        std::string cleanLogResult = "";

						// Get OCR result and clean it up for logging and matching------------------------------------------------------------------
                        char* rawOutText = g_OcrApi->GetUTF8Text();
                        if (rawOutText) {
                            std::string rawOcrStr(rawOutText);
                            free(rawOutText);
                            
                            std::string currentSegment = "";

                            for (char ch : rawOcrStr) {
                                if (ch == '\n' || ch == '\r' || ch == '\t') {
                                    if (!currentSegment.empty()) {
                                        if (!cleanLogResult.empty()) cleanLogResult += " + ";
                                        cleanLogResult += currentSegment;
                                        currentSegment = "";
                                    }
                                }
                                else {
                                    if (ch == ' ') {
                                        if (!currentSegment.empty() && currentSegment.back() != ' ') {
                                            currentSegment += ch;
                                        }
                                    }
                                    else {
                                        currentSegment += ch;
                                    }
                                }
                            }
                            if (!currentSegment.empty()) {
                                if (!cleanLogResult.empty()) cleanLogResult += " + ";
                                cleanLogResult += currentSegment;
                            }

                            if (!cleanLogResult.empty()) {
								// Print OCR result at most every 300ms to prevent log spamming
                                static auto lastLogPrintTime = std::chrono::steady_clock::now();
                                auto currentLogPrintTime = std::chrono::steady_clock::now();
                                auto elapsedLogMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentLogPrintTime - lastLogPrintTime).count();

                                if (elapsedLogMs >= 300) {
                                    if (g_DebugMode) {
                                        std::cout << "[OCR EYE] " << cleanLogResult << "\n";
                                    }
                                    lastLogPrintTime = currentLogPrintTime;
                                }
                            }
                        }

                        if (EvaluateMissionOCR(binWhite)) {
							// If it matches the whitelist, report immediately and add to reportedMissions to prevent duplicates
                            std::cout << "[MATCH] Found target in Blue-linked White Name at Y: " << absoluteY << "\n";
                            reportedMissions.push_back({ absoluteY, std::chrono::steady_clock::now() });
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            cv::waitKey(1);
                            return true;
                        }
                        else {
							// If it doesn't match the whitelist, we need to be more careful. We will add it to the waste fingerprint library, but only if it doesn't contain any high-value words from the whitelist. This way, we can quickly filter out obvious garbage in future frames without risking false positives on valuable missions.
                            bool isHighValueWord = false;
                            {
                                std::lock_guard<std::mutex> lock(g_ListMutex);
                                for (const auto& whiteWord : g_WhiteList) {
                                    std::string cleanWhite = whiteWord;
                                    cleanWhite.erase(std::remove(cleanWhite.begin(), cleanWhite.end(), ' '), cleanWhite.end());

                                    // Name list checker
                                    if (!cleanWhite.empty() && cleanLogResult.find(cleanWhite) != std::string::npos) {
                                        isHighValueWord = true;
                                        break;
                                    }
                                }
                            }
                            if (!isHighValueWord) {
                                g_WasteImgFingerprints.push_back(fingerprint);
                                if (g_WasteImgFingerprints.size() > 40) {
                                    g_WasteImgFingerprints.erase(g_WasteImgFingerprints.begin());
                                }
                            }
                            else {
								// If it contains high-value words but still doesn't match the whitelist, it's a borderline case. We will not add it to the waste library to avoid risking false positives, but we will also not report it. This way, we can keep an eye on it in future frames and see if it evolves into a clearer match or a clearer non-match.
                                g_WasteImgFingerprints.clear();
                            }
                        }
                    }
                }

				// Yellow text
                if (g_ScanMissionReward) {
                    int textX = static_cast<int>(60 * g_Scale);
                    int origY = static_cast<int>(blueY - (35 * g_Scale));
                    int textY = std::max(0, origY);
                    int textW = static_cast<int>(700 * g_Scale);
                    int textH = static_cast<int>(38 * g_Scale);

                    if (textX + textW > activeChannelFrame.cols) textW = activeChannelFrame.cols - textX;
                    if (textY + textH > activeChannelFrame.rows) textH = activeChannelFrame.rows - textY;

                    if (textX >= 0 && textW > 0 && textH > 0) {
                        cv::Rect goldRect(textX, textY, textW, textH);

                        cv::Mat colorTarget = activeChannelFrame(goldRect).clone();

                        if (g_DebugMode) {
                            cv::imshow("[Debug 2] Binarized Target", colorTarget);
                            cv::waitKey(1);
                        }

                        cv::Mat grayGoldTarget;
                        cv::cvtColor(colorTarget, grayGoldTarget, cv::COLOR_BGR2GRAY);

                        cv::Mat binGold;
                        cv::threshold(grayGoldTarget, binGold, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

                        int goldTotalPixels = binGold.cols * binGold.rows;
                        int goldTextPixels = goldTotalPixels - cv::countNonZero(binGold);

                        if (goldTextPixels < (100 * g_Scale)) continue;
                        if (goldTextPixels > (goldTotalPixels * 0.80)) continue;

                        // Finger print--------------------------------------------------------------------

                        cv::Mat fingerprintGold;
                        cv::resize(binGold, fingerprintGold, cv::Size(128, 8), 0, 0, cv::INTER_NEAREST);



                        bool isDuplicateGold = false;
                        for (const auto& oldFinger : g_WasteGoldFingerprints) {
                            cv::Mat diff;
                            cv::absdiff(fingerprintGold, oldFinger, diff);
                            double errorRatio = static_cast<double>(cv::countNonZero(diff)) / (128 * 8);

                            if (errorRatio < 0.03) { 
                                isDuplicateGold = true;
                                break;
                            }
                        }

                        if (isDuplicateGold) continue;

                        cv::resize(binGold, binGold, cv::Size(), 2.0, 2.0, cv::INTER_NEAREST);

                        std::string cleanGoldLogResult = "";

                        char* outTextGold = g_OcrApi->GetUTF8Text();
                        if (outTextGold) {
                            std::string ocrResultGold(outTextGold);
                            free(outTextGold);

                            std::string currentSegment = "";
                            for (char ch : ocrResultGold) {
                                if (ch == '\n' || ch == '\r' || ch == '\t') {
                                    if (!currentSegment.empty()) {
                                        if (!cleanGoldLogResult.empty()) cleanGoldLogResult += " + ";
                                        cleanGoldLogResult += currentSegment;
                                        currentSegment = "";
                                    }
                                }
                                else {
                                    if (ch == ' ') {
                                        if (!currentSegment.empty() && currentSegment.back() != ' ') {
                                            currentSegment += ch;
                                        }
                                    }
                                    else {
                                        currentSegment += ch;
                                    }
                                }
                            }
                            if (!currentSegment.empty()) {
                                if (!cleanGoldLogResult.empty()) cleanGoldLogResult += " + ";
                                cleanGoldLogResult += currentSegment;
                            }

                            if (!cleanGoldLogResult.empty()) {
                                static auto lastGoldLogTime = std::chrono::steady_clock::now();
                                auto currentGoldLogTime = std::chrono::steady_clock::now();
                                auto elapsedGoldMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentGoldLogTime - lastGoldLogTime).count();

                                if (elapsedGoldMs >= 300) {
                                    std::cout << "[REWARD EYE] " << cleanGoldLogResult << "\n";
                                    lastGoldLogTime = currentGoldLogTime;
                                }
                            }
                        }


                        if (EvaluateMissionOCR(binGold)) {
                            std::cout << "[MATCH] Found target in Blue-linked Gold Reward at Y: " << absoluteY << "\n";
                            reportedMissions.push_back({ absoluteY, std::chrono::steady_clock::now() });
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            cv::waitKey(1);
                            return true;
                        }
                        else {
                            bool isHighValueReward = false;
                            {
                                std::lock_guard<std::mutex> lock(g_ListMutex);
                                for (const auto& whiteWord : g_WhiteList) {
                                    std::string cleanWhite = whiteWord;
                                    cleanWhite.erase(std::remove(cleanWhite.begin(), cleanWhite.end(), ' '), cleanWhite.end());

                                    std::string upperGoldLog = cleanGoldLogResult;
                                    transform(upperGoldLog.begin(), upperGoldLog.end(), upperGoldLog.begin(), ::toupper);

                                    if (!cleanWhite.empty() && upperGoldLog.find(cleanWhite) != std::string::npos) {
                                        isHighValueReward = true;
                                        break;
                                    }
                                }
                            }
                            if (!isHighValueReward) {
                                g_WasteGoldFingerprints.push_back(fingerprintGold);
                                if (g_WasteGoldFingerprints.size() > 40) {
                                    g_WasteGoldFingerprints.erase(g_WasteGoldFingerprints.begin());
                                }
                            }
                            else {
                                g_WasteGoldFingerprints.clear();
                            }
                        }
                    }
                }
            }
        }
    }


    // Blue Mission Only is off------------------------------------------------------------------------------
    else {
        // Adhesive letter gaps
        cv::Mat dilateKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 1));

		// Wite text
        if (g_ScanMissionName) {
            cv::Mat dilatedWhite;
            cv::dilate(globalWhiteMask, dilatedWhite, dilateKernel);

            cv::Mat labels, stats, centroids;
            int numComponents = cv::connectedComponentsWithStats(dilatedWhite, labels, stats, centroids);
            for (int i = 1; i < numComponents; i++) {
                int compW = stats.at<int>(i, cv::CC_STAT_WIDTH);
                int compH = stats.at<int>(i, cv::CC_STAT_HEIGHT);

                if (compH < (9 * g_Scale) || compH >(17 * g_Scale) || compW < (50 * g_Scale)) continue;

                int absoluteY = stats.at<int>(i, cv::CC_STAT_TOP) + channelY;

                bool alreadyReported = false;
                for (const auto& item : reportedMissions) {
                    if (std::abs(item.first - absoluteY) < (25 * g_Scale)) { alreadyReported = true; break; }
                }
                if (alreadyReported) continue;

                int cropX = stats.at<int>(i, cv::CC_STAT_LEFT);
                int cropY = std::max(0, static_cast<int>(stats.at<int>(i, cv::CC_STAT_TOP) - static_cast<int>(5 * g_Scale)));
                int cropW = compW;
                int cropH = static_cast<int>((compH + 10) * g_Scale);

                if (cropX + cropW > activeChannelFrame.cols) cropW = activeChannelFrame.cols - cropX;
                if (cropY + cropH > activeChannelFrame.rows) cropH = activeChannelFrame.rows - cropY;

                if (cropX >= 0 && cropW > 0 && cropH > 0) {
                    cv::Rect cropRect(cropX, cropY, cropW, cropH);

                    cv::Mat colorTarget = activeChannelFrame(cropRect).clone();

                    if (g_DebugMode) {
                        cv::imshow("[Debug 2] Binarized Target", colorTarget);
                        cv::waitKey(1);
                    }

                    cv::Mat grayTarget;
                    cv::cvtColor(colorTarget, grayTarget, cv::COLOR_BGR2GRAY);

                    cv::Mat binWhite;
                    cv::threshold(grayTarget, binWhite, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

                    int totalPixels = binWhite.cols * binWhite.rows;
                    int textPixels = totalPixels - cv::countNonZero(binWhite);

                    if (textPixels < (150 * g_Scale)) continue;
                    if (textPixels > (totalPixels * 0.80)) continue;

					// Finger print--------------------------------------------------------------------
                    cv::Mat fingerprintWhite;
                    cv::resize(binWhite, fingerprintWhite, cv::Size(128, 8), 0, 0, cv::INTER_NEAREST);

                    bool isDuplicateWhite = false;
                    for (const auto& oldFinger : g_FullScanWhiteFingerprints) {
                        cv::Mat diff;
                        cv::absdiff(fingerprintWhite, oldFinger, diff);
                        double errorRatio = static_cast<double>(cv::countNonZero(diff)) / (128 * 8);

                        if (errorRatio < 0.03) { 
                            isDuplicateWhite = true;
                            break;
                        }
                    }

                    if (isDuplicateWhite) continue;

                    // OCR ----------------------------------------------------------------------------
                    cv::resize(binWhite, binWhite, cv::Size(), 2.0, 2.0, cv::INTER_NEAREST);

                    char* rawOutText = g_OcrApi->GetUTF8Text();
                    if (rawOutText) {
                        std::string rawOcrStr(rawOutText);
                        free(rawOutText);

                        std::string cleanText = "";
                        for (char ch : rawOcrStr) {
                            if (ch != '\n' && ch != '\r' && ch != '\t') {
                                if (ch == ' ') {
                                    if (!cleanText.empty() && cleanText.back() != ' ') cleanText += ch;
                                }
                                else {
                                    cleanText += toupper(ch);
                                }
                            }
                        }

                        if (!cleanText.empty()) {
                            if (g_DebugMode) {
                                std::cout << "[OCR EYE] " << cleanText << "\n";
                            }
                        }

                        if (EvaluateMissionOCR(binWhite)) {
                            std::cout << "[MATCH] Found target in Global Swept White Name at Y: " << absoluteY << "\n";
                            reportedMissions.push_back({ absoluteY, std::chrono::steady_clock::now() });
                            anyTargetFound = true;
                            std::this_thread::sleep_for(std::chrono::milliseconds(150));
                            cv::waitKey(1);
                            return true;
                        }
                        else {
                            bool isHighValueWord = false;

                            {
                                std::lock_guard<std::mutex> lock(g_ListMutex);
                                for (const auto& whiteWord : g_WhiteList) {
                                    std::string cleanWhite = whiteWord;
                                    cleanWhite.erase(std::remove(cleanWhite.begin(), cleanWhite.end(), ' '), cleanWhite.end());

                                    if (!cleanWhite.empty() && cleanText.find(cleanWhite) != std::string::npos) {
                                        isHighValueWord = true;
                                        break;
                                    }
                                }
                            }

                            if (!isHighValueWord) {
                                g_FullScanWhiteFingerprints.push_back(fingerprintWhite);
                                if (g_FullScanWhiteFingerprints.size() > 50) {
                                    g_FullScanWhiteFingerprints.erase(g_FullScanWhiteFingerprints.begin());
                                }
                            }
                            else {
                                g_FullScanWhiteFingerprints.clear();
                            }
                        }
                    }
                }
            }
        }

		// Yellow text
        if (g_ScanMissionReward) {
            cv::Mat dilatedGold;
            cv::dilate(globalGoldMask, dilatedGold, dilateKernel);

            cv::Mat labels, stats, centroids;
            int numComponents = cv::connectedComponentsWithStats(dilatedGold, labels, stats, centroids);
            for (int i = 1; i < numComponents; i++) {
                int compW = stats.at<int>(i, cv::CC_STAT_WIDTH);
                int compH = stats.at<int>(i, cv::CC_STAT_HEIGHT);

                if (compH < (8 * g_Scale) || compH >(30 * g_Scale) || compW < (30 * g_Scale) || compW >(500 * g_Scale)) continue;

                int absoluteY = stats.at<int>(i, cv::CC_STAT_TOP) + channelY;

                bool alreadyReported = false;
                for (const auto& item : reportedMissions) {
                    if (std::abs(item.first - absoluteY) < (25 * g_Scale)) { alreadyReported = true; break; }
                }
                if (alreadyReported) continue;

                int origLeft = stats.at<int>(i, cv::CC_STAT_LEFT);
                int origTop = stats.at<int>(i, cv::CC_STAT_TOP);

                int cropX = origLeft;
                int cropY = std::max(0, static_cast<int>(origTop - static_cast<int>(7 * g_Scale)));
                int cropW = compW;
                int cropH = static_cast<int>(35 * g_Scale);

                if (cropX + cropW > activeChannelFrame.cols) cropW = activeChannelFrame.cols - cropX;
                if (cropY + cropH > activeChannelFrame.rows) cropH = activeChannelFrame.rows - cropY;

                if (cropX >= 0 && cropW > 0 && cropH > 0) {
                    cv::Rect cropRect(cropX, cropY, cropW, cropH);

                    cv::Mat colorTarget = activeChannelFrame(cropRect).clone();

                    if (g_DebugMode) {
                        cv::imshow("[Debug 2] Binarized Target", colorTarget);
                        cv::waitKey(1);
                    }

                    cv::Mat grayGoldTarget;
                    cv::cvtColor(colorTarget, grayGoldTarget, cv::COLOR_BGR2GRAY);

                    cv::Mat binGold;
                    cv::threshold(grayGoldTarget, binGold, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

                    int goldTotalPixels = binGold.cols * binGold.rows;
                    int goldTextPixels = goldTotalPixels - cv::countNonZero(binGold);

                    if (goldTextPixels < (100 * g_Scale)) continue;
                    if (goldTextPixels > (goldTotalPixels * 0.80)) continue;

                    //Finger print
                    cv::Mat fingerprintGold;
                    cv::resize(binGold, fingerprintGold, cv::Size(128, 8), 0, 0, cv::INTER_NEAREST);

                    bool isDuplicateGold = false;
                    for (const auto& oldFinger : g_FullScanGoldFingerprints) {
                        cv::Mat diff;
                        cv::absdiff(fingerprintGold, oldFinger, diff);
                        double errorRatio = static_cast<double>(cv::countNonZero(diff)) / (128 * 8);

                        if (errorRatio < 0.03) {
                            isDuplicateGold = true;
                            break;
                        }
                    }

                    if (isDuplicateGold) continue;

                    // OCR
                    cv::resize(binGold, binGold, cv::Size(), 2.0, 2.0, cv::INTER_NEAREST);

                    std::string cleanGoldLogResult = "";

                    char* outTextGold = g_OcrApi->GetUTF8Text();
                    if (outTextGold) {
                        std::string ocrResultGold(outTextGold);
                        free(outTextGold);

                        std::string currentSegment = "";
                        for (char ch : ocrResultGold) {
                            if (ch == '\n' || ch == '\r' || ch == '\t') {
                                if (!currentSegment.empty()) {
                                    if (!cleanGoldLogResult.empty()) cleanGoldLogResult += " + ";
                                    cleanGoldLogResult += currentSegment;
                                    currentSegment = "";
                                }
                            }
                            else {
                                if (ch == ' ') {
                                    if (!currentSegment.empty() && currentSegment.back() != ' ') {
                                        currentSegment += ch;
                                    }
                                }
                                else {
                                    currentSegment += ch;
                                }
                            }
                        }
                        if (!currentSegment.empty()) {
                            if (!cleanGoldLogResult.empty()) cleanGoldLogResult += " + ";
                            cleanGoldLogResult += currentSegment;
                        }

                        if (!cleanGoldLogResult.empty()) {
                            static auto lastGoldLogTime = std::chrono::steady_clock::now();
                            auto currentGoldLogTime = std::chrono::steady_clock::now();
                            auto elapsedGoldMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentGoldLogTime - lastGoldLogTime).count();

                            if (elapsedGoldMs >= 300) {
                                std::cout << "[REWARD EYE] " << cleanGoldLogResult << "\n";
                                lastGoldLogTime = currentGoldLogTime;
                            }
                        }
                    }

                    if (EvaluateMissionOCR(binGold)) {

                        std::cout << "[MATCH] Found target in Global Swept Gold Reward at Y: " << absoluteY << "\n";
                        reportedMissions.push_back({ absoluteY, std::chrono::steady_clock::now() });
                        anyTargetFound = true;
                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                        cv::waitKey(1);
                        return true;
                    }
                    else {
                        bool isHighValueReward = false;
                        {
                            std::lock_guard<std::mutex> lock(g_ListMutex);
                            for (const auto& whiteWord : g_WhiteList) {
                                std::string cleanWhite = whiteWord;
                                cleanWhite.erase(std::remove(cleanWhite.begin(), cleanWhite.end(), ' '), cleanWhite.end());

                                std::string upperGoldLog = cleanGoldLogResult;
                                transform(upperGoldLog.begin(), upperGoldLog.end(), upperGoldLog.begin(), ::toupper);

                                if (!cleanWhite.empty() && upperGoldLog.find(cleanWhite) != std::string::npos) {
                                    isHighValueReward = true;
                                    break;
                                }
                            }
                        }

                        if (!isHighValueReward) {
                            g_FullScanGoldFingerprints.push_back(fingerprintGold);
                            if (g_FullScanGoldFingerprints.size() > 50) {
                                g_FullScanGoldFingerprints.erase(g_FullScanGoldFingerprints.begin());
                            }
                        }
                        else {
                            g_FullScanGoldFingerprints.clear();
                        }
                    }
                }
            }
        }
    }
    static int frameCounter = 0;
    frameCounter++;
    if (frameCounter >= 5) {
        cv::waitKey(1);
        frameCounter = 0;
    }
    return anyTargetFound;
}



cv::Mat GetGameScreenDynamic(HWND hwnd, int x, int y, int w, int h) {
    if (!IsWindow(hwnd)) return cv::Mat();

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int absX = rect.left + x;
    int absY = rect.top + y;

	// CCTV Refresh: Always create fresh DC and Bitmap objects for each capture to ensure clean memory and prevent leaks that cause freezing over time.
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, w, h);

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, w, h, hScreenDC, absX, absY, SRCCOPY | CAPTUREBLT);

    GdiFlush();

    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

	// Create a Mat header that points directly to the bitmap data, allowing OpenCV to manage it without copying. This is more efficient and ensures proper cleanup when the Mat goes out of scope.
    cv::Mat src(h, w, CV_8UC3);
    GetDIBits(hMemoryDC, hBitmap, 0, h, src.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    SelectObject(hMemoryDC, hOldBitmap);

    // Kill Windows GDI
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return src;
}

HWND FindEliteDangerousWindow() {
    HWND hwnd = FindWindowA("EliteDangerousWinClass", nullptr);
    if (!hwnd) hwnd = FindWindowA(nullptr, "Elite - Dangerous (CLIENT)");
    if (!hwnd) hwnd = FindWindowA(nullptr, "Elite - Dangerous");
    if (!hwnd) hwnd = FindWindowA(nullptr, "Elite Dangerous");
    if (!hwnd) hwnd = FindWindowA("EliteDangerous64", nullptr);
    return hwnd;
}

// Initialize the OCR engine======================================================================================================================
bool InitializeOCR() {
    std::cout << "[OCR INIT] Synchronizing optical semantic layers...\n";

    // 1. Windows Hardware Telemetry: Directly grab the absolute physical path of this running .exe
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    // Convert to string and instantly normalize ALL chaotic backslashes '\\' into clean standard forward slashes '/'
    std::string currentDir(exePath);
    std::replace(currentDir.begin(), currentDir.end(), '\\', '/');

    // Slice off the file name to isolate the pure root folder path string
    size_t lastSlash = currentDir.find_last_of("/");
    if (lastSlash != std::string::npos) {
        currentDir = currentDir.substr(0, lastSlash); // Safely extracts path up to your Release or App folder
    }

    //  2. The Golden Solution (The Slashless /tessdata Injection):
    // According to Tesseract API architecture, we directly forge the absolute path straight to the "tessdata" folder itself.
    // CRITICAL REQUIREMENT: The trailing slash MUST be removed! (e.g., must be ".../Elite Mission Radar_v7.5/tessdata")
    // This strictly forces Tesseract to open 'eng.traineddata' inside that folder without any parent-directory warping!
    std::string ocrTargetFolder = currentDir + "/tessdata";

    g_OcrApi = new tesseract::TessBaseAPI();

    //  The Absolute Cure: Injects the bulletproof clean absolute directory string straight into the engine's core!
    if (g_OcrApi->Init(ocrTargetFolder.c_str(), "eng", tesseract::OEM_DEFAULT)) {
        std::cerr << "[FATAL ERROR] Could not initialize Tesseract OCR engine.\n";
        std::cerr << "              Current Dynamic Resolution Attempt Path: " << ocrTargetFolder << "/\n";
        std::cerr << "              Please ensure 'eng.traineddata' exists inside that exact subfolder!\n";

        system("pause");
        return false;
    }

    // ---------------------------------------------------------------------------------------------------------
    g_OcrApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    g_OcrApi->SetVariable("tessedit_parallel", "0");
    g_OcrApi->SetVariable("OMP_NUM_THREADS", "1");
    g_OcrApi->SetVariable("tessedit_char_whitelist", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$,.-/ ");

    g_OcrApi->SetVariable("debug_acceptable_w_samples", "0");
    g_OcrApi->SetVariable("textord_debug_tabfind", "0");

    // Your Core Overclock Switches
    g_OcrApi->SetVariable("load_system_dawg", "0");
    g_OcrApi->SetVariable("load_freq_dawg", "0");
    g_OcrApi->SetVariable("tessedit_enable_doc_white_list", "1");

    g_OcrApi->SetVariable("tessedit_parallel", "0");
    g_OcrApi->SetVariable("OMP_NUM_THREADS", "1");

    std::cout << "[NOMINAL] Tesseract OCR online. Semantic matrix locked safely.\n\n";
    return true;
}


//  int main() SOURCE CODE========================================AI DO IT, OFCOURSE. Who has time to write so many introductory lines?

int main() {
    //  Critical Defense: Force Windows kernel to report 100% true physical pixels, smashing the 150% / 200% DPI text scaling trap.
    SetProcessDPIAware();

// ==============================================================================================================================================================================================
//  HARDCORE LEGAL GATEKEEPER: MANDATORY EULA AFFIRMATIVE INPUT CHECK (v7.5 - THE AT-YOUR-OWN-RISK AMENDMENT)
// ==============================================================================================================================================================================================
    const std::string CURRENT_VERSION = "v7.5";
    const std::string EULA_FILE_NAME = "eula_accepted.dat";
    bool needToAcceptEula = true;

    // 1. Check if the local license authorization file already exists
    std::ifstream eulaCheck(EULA_FILE_NAME);
    if (eulaCheck.is_open()) {
        std::string savedVersion;
        std::getline(eulaCheck, savedVersion);
        eulaCheck.close();

        // If the version signature matches perfectly, bypass the block smoothly
        if (savedVersion == CURRENT_VERSION) {
            needToAcceptEula = false;
        }
    }

    //  If signature is corrupted, missing, or outdated, trigger the mandatory legal gateway
    if (needToAcceptEula) {
        while (true) {
            // Force console clearance to isolate the legal text
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif

            std::cout << "========================================================================================================\n";
            std::cout << "                                   END USER LICENSE AGREEMENT (EULA) & DISCLAIMER                      \n";
            std::cout << "========================================================================================================\n\n";
            std::cout << "IMPORTANT: READ CAREFULLY BEFORE PROCEEDING. BY USING THIS COMMAND-LINE SOFTWARE UTILITY (THE \"SOFTWARE\"),\n";
            std::cout << "YOU EXPLICITLY AGREE TO BE BOUND BY EVERY TERM OF THIS LEGALLY BINDING CONTRACT. IF YOU DO NOT AGREE, \n";
            std::cout << "YOU MUST IMMEDIATELY EXIT AND DELETE THE SOFTWARE.\n\n";

            std::cout << "[STRICTLY PASSIVE PRODUCT DEFINITION]\n";
            std::cout << "The Software is provided strictly as a local, standalone screen-scraping utility that utilizes \n";
            std::cout << "Optical Character Recognition (OCR) and localized audio notification signals. The Software does not \n";
            std::cout << "connect to the internet, does not harvest personal data, does not intercept network packets, does not \n";
            std::cout << "inject code into any third-party memory space, and does not alter any third-party software data.\n\n";

            std::cout << "[ZERO-AUTOMATION & PROHIBITION OF MODIFICATIONS]\n";
            std::cout << "THE SOFTWARE CONTAINS NO AUTOMATION, MOUSE/KEYBOARD EMULATION, OR GAMEPLAY BOT MACROS. You are granted \n";
            std::cout << "a restrictive, non-transferable license to use the Software solely in its native, unmodified state. \n";
            std::cout << "You are strictly prohibited from reverse-engineering, decompiling, or patching the executable binary. \n";
            std::cout << "Any modification, injection of automated clicking routines, or coupling of this Software with external \n";
            std::cout << "automated scripts is an unauthorized tampering breach of this contract. All civil and criminal \n";
            std::cout << "liabilities resulting from such unauthorized modifications rest entirely on the individual user.\n\n";

            std::cout << "[THE FRONTIER COMPLIANCE & ACCOUNT BAN CLAUSE]\n";
            std::cout << "THE SOFTWARE IS DESIGNED TO BE 100% PASSIVE AND TECHNICALLY COMPLIES WITH THE RELEVANT SECTIONS OF THE \n";
            std::cout << "ELITE DANGEROUS EULA REGULATIONS (AS IT DOES NOT HOOK RAM OR MODIFY GAME PACKETS). IN THEORY, YOU SHOULD NOT \n";
            std::cout << "HAVE ANY REASON TO WORRY ABOUT GETTING BANNED. HOWEVER, GAME ADMINISTRATORS RETENTION ULTIMATE ARBITRATION RIGHTS. \n";
            std::cout << "IN THE EXTREMELY UNLIKELY EVENT THAT YOUR ACCOUNT INCURS SANCTIONS, STRIKES, OR PERMANENT TERMINATION, THE DEVELOPER \n";
            std::cout << "BEARS ABSOLUTELY ZERO LIABILITY. ALL RISKS REST ENTIRELY ON YOU AND YOUR INDIVIDUAL LUCK. USE AT YOUR OWN RISK.\n\n";

            std::cout << "[ABSOLUTE ASSUMPTION OF RISK & NO LIABILITY]\n";
            std::cout << "THE DEVELOPER DISCLAIMS ALL LIABILITY FOR ANY DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES, LOSS OF DIGITAL \n";
            std::cout << "ASSETS, OR ACCOUNT TERMINATIONS ARISING FROM THE USE OF THIS SOFTWARE. THE SOFTWARE IS PROVIDED \"AS IS\" \n";
            std::cout << "WITHOUT WARRANTIES OF ANY KIND.\n\n";

            std::cout << "[SEVERABILITY AND GOVERNING LAW]\n";
            std::cout << "If any provision of this Agreement is found unenforceable by a court of competent jurisdiction, the \n";
            std::cout << "remaining provisions will continue in full force and effect. This Agreement constitutes the entire \n";
            std::cout << "contract between the Developer and the User regarding the Software, superseding all prior discussions \n";
            std::cout << "or marketing representations.\n\n";

            std::cout << "--------------------------------------------------------------------------------------------------------\n";
            std::cout << "ACKNOWLEDGMENT OF MANDATORY ACCEPTANCE:\n";
            std::cout << "Do you voluntarily accept and bind yourself to all terms of this EULA agreement? (Y/N): ";

            std::string userAcceptInput;
            std::cin >> userAcceptInput;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                continue;
            }

            if (userAcceptInput == "Y" || userAcceptInput == "y") {
                std::ofstream eulaWrite(EULA_FILE_NAME);
                if (eulaWrite.is_open()) {
                    eulaWrite << CURRENT_VERSION << "\n";
                    eulaWrite.close();
                }
                std::cout << "\n[EULA ACCEPTED] Secure cryptographic hardware gateway passed. Initializing radar registers...\n\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

#ifdef _WIN32
                std::system("cls");
#else
                std::system("clear");
#endif

                break;
            }
            else if (userAcceptInput == "N" || userAcceptInput == "n") {
                std::cout << "\n[EULA DECLINED] Activation denied. You must agree to the contract terms to deploy this utility.\n";
                std::cout << "Exiting system in 3 seconds...\n";
                std::this_thread::sleep_for(std::chrono::seconds(3));
                return 0;
            }
            else {
                std::cout << "\n[INVALID INPUT] Technical gate protocol unrecognized. Please type 'Y' to accept or 'N' to decline.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }

    std::cout << "======================================================================" << endl;
    std::cout << "      INITIALIZING: ELITE MISSION REWARD FILTER & SCANNER [v7.5]       " << endl;
    std::cout << "======================================================================" << endl;
    std::cout << endl;

    // 1. Core Startup: Auto-load configurations from hard drive disk config.txt
    LoadConfigFromDisk();

    std::cout << "[PRE-FLIGHT CHECKLIST] Please verify all conditions before proceeding:" << endl;
    std::cout << endl;
    std::cout << "1. COMPLIANCE & SAFETY:" << endl;
    std::cout << "   - 100% passive optical screen analyzer utilizing low-level Windows API." << endl;
    std::cout << "   - Fully standalone, async input tracking (completely ban-safe, 0ms latency)." << endl;
    std::cout << endl;
    std::cout << "2. CONTROL CONFIGURATION:" << endl;
    std::cout << "   - Manually scroll your mission board or hold down controller D-Pad in-game." << endl;
    std::cout << "   - Press [H] / [L] anytime on this console window to adjust audio volume." << endl;
    std::cout << "   - Press [/] anytime on this console window to open Command Line Interface." << endl;
    std::cout << endl;
    std::cout << "3. AUDIBLE PULSE STATUS MONITORS:" << endl;
    std::cout << "   - Low Pitch Beep (Do)   : Radar throughput is optimal (6+ FPS). Fingerprint" << endl;
    std::cout << "                              caches are actively processing. ～0% task loss risk." << endl;
    std::cout << "   - High Pitch Alert (Bi)  : *CRITICAL HYPOXIA ALERT*! Performance dropped below 5 FPS!" << endl;
    std::cout << "                              Radar scanning is severely lagging. Severe task dropping" << endl;
    std::cout << "                              might occur! Please adjust your filters immediately." << endl;
    std::cout << endl;
    std::cout << "4. PROCESSOR OVERLOAD WARNING:" << endl;
    std::cout << "   - Avoid overly broad scan ranges to ensure seamless tracking stability." << endl;
    std::cout << "   - Enforcing GLOBAL TEXT SWEEP while enabling BOTH White Name and Gold Reward" << endl;
    std::cout << "     scanners simultaneously will trigger massive multi-matrix overhead, forcing" << endl;
    std::cout << "     severe bottlenecking on the OCR engine and causing a radical drop in FPS." << endl;
    std::cout << "   - To maintain peak operational throughput, please isolate your tracking string" << endl;
    std::cout << "     targets or keep [BLUE CIRCLE PRIORITY] enabled at all times." << endl;
    std::cout << "----------------------------------------------------------------------" << endl;
    std::cout << "[DONATION & DOCUMENTATION]" << endl;
    std::cout << "   - Type '[info]' in command line mode to view active EULA & licensing details." << endl;
    std::cout << "   - Support the developer / buy a coffee: https://ko-fi.com/fleogendepigge" << endl;
    std::cout << "======================================================================" << endl;
    std::cout << endl;

    // 3. Call Integration Wizard: Automatically executes full delay and scanning configuration setup
    RunFullSetupWizard();

    // 4. Initialize hardware audio devices
    ApplyHardwareVolume(g_CurrentVolume);

    // 5. Fire up the Tesseract OCR sub-core engine
    if (!InitializeOCR()) return -1;

    std::cout << "\n[RADAR ACTIVE] System armed. Switch to Elite Dangerous and begin scanning!" << endl;
    std::cout << "[INFO] Active session keys: [W/S] or D-Pad. Press [/] anytime for CLI Command Mode.\n";
    std::cout << "                            [H] / [L] On this console window to adjust audio volume." << endl;

    while (g_IsRunning) {

        HWND eliteHwnd = FindEliteDangerousWindow();

        static auto lastFpsTime = std::chrono::steady_clock::now();
        static int fpsLoopCount = 0;

        // A. Console window focus input interceptor (100% safe from in-game binding conflicts)
        if (_kbhit()) {
            int ch = _getch();

            // 【CLI Trigger】 Enter Command Line Mode when player hits [/] - Wrapped as a hardware PAUSE BUTTON
            if (ch == '/') {
                g_CommandMode = true; // Freeze the background scanning thread instantly

                std::cout << "\n========================================================================================================\n";
                std::cout << "[RADAR PAUSED] Active Sweep Suspended Successfully. Universal CLI Command Center Active.            \n";
                std::cout << "========================================================================================================\n";
                std::cout << " [setup]    : Re-trigger the complete 4-step setup flow wizard (Set speed, modes, and keyword packs).\n";
                std::cout << " [re]       : Quick rerun wizard. Bypasses FPS delay setup to directly switch sweep modes or packs.\n";
                std::cout << " [resize]   : Recalibrate bounds. Instantly re-probes game client-area window pixels to re-lock scale.\n";
                std::cout << " [bind]     : Customize control keys. Re-assign radar scrolling hooks between W/S, MButton, or XButton.\n";
                std::cout << " [speed]    : Hardware limit tuning. Manually overwrite your background screenshots throttling limit.\n";
                std::cout << " [mute]     : Toggle audio tracking pulse. Switches the 333ms background nominal heartbeat sound on/off.\n";
                std::cout << " [test]     : Test alert sound. Plays the high-value target detected siren (Airhorn) instantly for volume checks.\n";
                std::cout << " [listw]    : table viewer. Displays active accepted White mission targets stored in registers.\n";
                std::cout << " [listb]    : table viewer. Displays active filtered/ignored Black keywords stored in registers.\n";
                std::cout << " [addw]     : Hot-inject new keyword into WHITE list. Forces immediate 0ms sync and flashes fingerprint memory.\n";
                std::cout << " [addb]     : Hot-inject new keyword into BLACK list. Forces immediate 0ms sync and flashes fingerprint memory.\n";
                std::cout << " [delw]     : Hot-remove exact keyword from WHITE list. Instantly clears active caches to prevent misreads.\n";
                std::cout << " [delb]     : Hot-remove exact keyword from BLACK list. Instantly clears active caches to prevent misreads.\n";
                std::cout << " [remember] : Save variables. Permanently write and hard-bake all live memory modifications into 'config.txt'.\n";
                std::cout << " [info]     : View 4-Step Dummy-Proof Guide, Developer Support/Donation channel & Legal Frontier EULA.\n";
                std::cout << " [debug]    : Toggle universal diagnostic mode (Forces active FPS monitor, text outputs & 3 or 4 viewports).\n";
                std::cout << "                *CRITICAL WARNING*: Activating this mode will SEVERELY and radically drop your radar FPS\n";
                std::cout << "                                    due to intense graphics buffer copying! DO NOT enable during routine grinds!\n";
                std::cout << " [stop]     : Clean termination. purges active Tesseract threads and exits the program safely.\n";
                std::cout << " [/]        : Unpause input. Close command center and instantly resume full-speed background radar loop.\n";
                std::cout << "--------------------------------------------------------------------------------------------------------\n";
                std::cout << "*NOTICE*: Closing the utility by clicking the standard window top-right [X] button is also completely safe.\n";
                std::cout << "========================================================================================================\n";
                std::cout << "Enter Command: ";

                std::string cmd;
                std::cin >> cmd;

                if (cmd == "setup") {
                    RunFullSetupWizard();
                }
                else if (cmd == "re") {
                    std::cout << "\n[RE-TRIGGER WIZARD] Rerunning Setup Wizard (FPS Configuration Skipped)...\n";
                    int backupDelay = g_PotatoScanDelayMs;
                    RunFullSetupWizard();

                    g_PotatoScanDelayMs = backupDelay;

                    std::cout << "[PRESET RELOADED] Successfully re-synchronized wizard parameters into memory!\n";
                }
                else if (cmd == "resize") {
                    //  Automated recaching shortcut: Dynamically re-probes hardware coordinates on the fly!
                    if (IsWindow(eliteHwnd)) {
                        RECT clientRect;
                        GetClientRect(eliteHwnd, &clientRect);
                        g_TargetWidth = clientRect.right - clientRect.left;
                        g_TargetHeight = clientRect.bottom - clientRect.top;
                        g_Scale = ((static_cast<double>(g_TargetWidth) / 1920.0) + (static_cast<double>(g_TargetHeight) / 1200.0)) / 2.0;
                        std::cout << "[SUCCESS] Radar dynamic eye successfully re-locked client dimensions: " << g_TargetWidth << "x" << g_TargetHeight << "\n";
                        SaveConfigToDisk();
                    }
                    else {
                        std::cout << "[ERROR] Game window untracked! Unable to execute optical auto-calibration.\n";
                    }
                }
                else if (cmd == "test") {
                    std::cout << "\n[AUDIO TEST] Blasting target detected alert sound (Airhorn)..." << std::endl;

                    PlayTargetDetectedAlertAsync();

                    std::cout << "[SUCCESS] Audio trigger executed. If you didn't hear anything, please tune your volume [H/L] or check Windows mixer.\n\n";
                }
                else if (cmd == "bind") {
                    std::cout << "\n==================================================\n";
                    std::cout << "[BIND MENU] Please choose your primary scanning trigger method:\n";
                    std::cout << "  1. Live Key-Catching     (Press ANY 2 keys on keyboard to trigger radar)\n";
                    std::cout << "  2. Default Reset         (Restore standard W / S)\n";
                    std::cout << "  3. Mouse Side Buttons    (VK_XBUTTON1 & 2 - Thumb Buttons Multi-Channel)\n";
                    std::cout << "==================================================\n";
                    std::cout << "Select option (1-3): ";

                    int bChoice = 0;
                    std::cin >> bChoice;

                    if (std::cin.fail()) { std::cin.clear(); std::cin.ignore(10000, '\n'); bChoice = 0; }

                    if (bChoice == 1) {
                        while (_kbhit()) { _getch(); }
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));

                        std::cout << "\n[LISTENING] Please press your [ 1st Radar Trigger Key ] now...";
                        int caughtUpKey = 0;
                        while (true) {
                            for (int vk = 0x07; vk <= 0xFE; vk++) {
                                if (GetAsyncKeyState(vk) & 0x8000) { caughtUpKey = vk; break; }
                            }
                            if (caughtUpKey != 0) break;
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        g_BindKeyUp = caughtUpKey;
                        std::cout << "\n[SUCCESS] Caught key code: 0x" << std::hex << std::uppercase << g_BindKeyUp << " and bound to 1st Trigger!\n";

                        std::this_thread::sleep_for(std::chrono::milliseconds(400));
                        while (GetAsyncKeyState(g_BindKeyUp) & 0x8000) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }

                        std::cout << "[LISTENING] Please press your [ 2nd Radar Trigger Key ] now...";
                        int caughtDownKey = 0;
                        while (true) {
                            for (int vk = 0x07; vk <= 0xFE; vk++) {
                                if (GetAsyncKeyState(vk) & 0x8000) { caughtDownKey = vk; break; }
                            }
                            if (caughtDownKey != 0) break;
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        g_BindKeyDown = caughtDownKey;
                        std::cout << "\n[SUCCESS] Caught key code: 0x" << std::hex << std::uppercase << g_BindKeyDown << " and bound to 2nd Trigger!\n";

                        std::cout << std::dec;
                        std::cout << "[COMPLETE] Both keys are bound. Pressing EITHER key in-game will fully activate radar sweep!\n";
                    }
                    else if (bChoice == 2) {
                        g_BindKeyUp = 'W'; g_BindKeyDown = 'S';
                        std::cout << "[SUCCESS] Restored default W and S !\n";
                    }
                    else if (bChoice == 3) {
                        g_BindKeyUp = VK_XBUTTON1; g_BindKeyDown = VK_XBUTTON1;
                        std::cout << "[SUCCESS] Bound to BOTH Mouse Side Buttons (Pressing either Forward or Backward triggers scanning)!\n";
                    }
                    else {
                        std::cout << "[ERROR] Invalid selection. Aborting configuration.\n";
                    }
                }
                else if (cmd == "speed") {
                    int sChoice = 0;
                    do {
                        std::cout << "\n[REFRESH RATE PROFILE] Select new hardware speed profile (1-3):\n";
                        std::cout << "  1. Ludicrous Mode (45+ FPS - Maximum sweep speed for high-end gaming PC)\n";
                        std::cout << "  2. Safe Mode      (25 FPS  - Balanced capture rate for standard multi-tasking)\n";
                        std::cout << "  3. Potato PC Mode (10 FPS  - Ultra-low CPU snapshot overhead for legacy laptops)\n";
                        std::cout << "  *NOTICE*: These values represent maximum CAP LIMITS enforced to protect your CPU from overheating.\n";
                        std::cout << "            Actual performance depends heavily on your hardware specs AND your active scanning methods.\n";
                        std::cout << "Select speed profile (1-3): ";
                        std::cin >> sChoice;

                        if (std::cin.fail()) {
                            std::cin.clear(); std::cin.ignore(10000, '\n');
                            sChoice = 0;
                        }
                        if (sChoice < 1 || sChoice > 3) {
                            std::cout << "[INVALID INPUT] Out of range! Please enter 1, 2, or 3.\n";
                        }
                    } while (sChoice < 1 || sChoice > 3);

                    // Hard-bake hardware delay profiles based on real-time screen capture throughput
                    if (sChoice == 2) g_PotatoScanDelayMs = 30;
                    else if (sChoice == 3) g_PotatoScanDelayMs = 90;
                    else g_PotatoScanDelayMs = 0;

                    SaveConfigToDisk(); // Automatically backup variables to config.txt
                    std::cout << "[SUCCESS] Hardware scan frequency successfully synchronized and updated!\n";
                }
                else if (cmd == "mute") {
                    g_MutePulse = !g_MutePulse;
                    if (g_MutePulse) {
                        std::cout << "[SUCCESS] Radar tracking pulse [Beep] is now MUTED. (Silent Sweep Mode)\n";
                    }
                    else {
                        std::cout << "[INFO] Radar tracking pulse [Beep] has been RESTORED (500ms Interval).\n";
                    }
                }
                else if (cmd == "remember") {
                    SaveConfigToDisk();
                    std::cout << "[SUCCESS] All custom list updates successfully written to '.\\config.txt'!\n";
                }
                else if (cmd == "clear") {
                    std::lock_guard<std::mutex> lock(g_ListMutex);

                    g_WasteImgFingerprints.clear();
                    g_WasteGoldFingerprints.clear();
                    g_FullScanWhiteFingerprints.clear();
                    g_FullScanGoldFingerprints.clear();

                    std::cout << "\n[CACHE PURGED] All historical task fingerprints have been completely flushed from memory!\n";
                    std::cout << "                 Radar will force-re-scan all visible rows on the next frame.\n";
                }
                else if (cmd == "debug") {
                    g_DebugMode = !g_DebugMode; // Toggle the universal switch

                    if (g_DebugMode) {
                        std::cout << "\n[CRITICAL WARNING] UNIVERSAL DEBUG MODE ACTIVATED!\n";
                        std::cout << "   - Live FPS logs, OCR text rendering, and 3 window displays are now FORCED ON.\n";
                        std::cout << "   - *EXTREME PERFORMANCE PENALTY*: This mode will SEVERELY drop your radar FPS\n";
                        std::cout << "     due to heavy GPU/GDI memory copying and rendering bottlenecks!\n";
                        std::cout << "   - Please DO NOT leave this enabled during normal gameplay or background grinding.\n\n";

                        std::cout << "  *HIDDEN DEBUG CMD*: [clear] : Type 'clear' to instantly wipe all saved tasks from radar memory.\n";
                        std::cout << "  *50 STORAGE SLOTS*: The radar reserves exactly 50 memory slots to remember the last 50 scanned result.\n";
                        std::cout << "                      If a task is identical to any of these 50 saved result, [Debug Window 2] will NOT\n";
                        std::cout << "                      show it again, and the raw text will NOT print repeatedly on your console screen.\n";
                        std::cout << "  *WHY DO THIS?    *: Because image-to-text recognition (OCR) is a very time-consuming step.\n";
                        std::cout << "                      Therefore, we flatten the image into a fingerprint first (which takes almost zero time)\n";
                        std::cout << "                      to exclude those scanning results that have already been marked as invalid beforehand.\n";
                        std::cout << "  *NO WORRIES      *: Any custom modification to your White/Black list will INSTANTLY flush this fingerprint\n";
                        std::cout << "                      storage memory. You can safely update your target keywords anytime during operations.\n";
                    }
                    else {
                        cv::destroyAllWindows();
                        std::cout << "\n[NOMINAL MODE] Debug mode disabled. All diagnostic matrices closed.\n";
                        std::cout << "                 Radar execution has returned to peak 100% hardware efficiency!\n";
                    }
                    SaveConfigToDisk(); // Save choice to config.txt
                }
                else if (cmd == "info") {
                    std::cout << "\n========================================================================================================\n";
                    std::cout << "                         ELITE MISSION RADAR v7.5 - THE ESSENTIAL USER MANUAL & FAQ                     \n";
                    std::cout << "========================================================================================================\n";

                    std::cout << "\n[1. HOW IT WORKS IN PLAIN ENGLISH]\n";
                    std::cout << "     SUPER-FAST SCREEN SNIPER: This radar takes dozens of screenshots per second in the background,\n";
                    std::cout << "     constantly reading everything on your board. If it finds your target keywords, IT WILL ALERT YOU IMMEDIATELY.\n";
                    std::cout << "     THE TELEMETRY BEATING: While you are scrolling, the background pulse tells you everything:\n";
                    std::cout << "    - Low Pitch Beep      : Everything is running perfectly fast. Zero risk of missing any missions.\n";
                    std::cout << "    - High Pitch Alert    : Your radar is lagging! This means your system is running too slow.\n";
                    std::cout << "                            Pay close attention, as the radar might drop tasks when running slow.\n";

                    std::cout << "\n[2. THE GOLDEN RULES - OR THE BOT GOES COMPLETELY BLIND]\n";
                    std::cout << "    CLASSIC ORANGE HUD ONLY: This bot ONLY reads the original, standard classic orange mission board.\n";
                    std::cout << "    If you changed your HUD colors using EDHM or other graphics mods, THE BOT WILL GO COMPLETELY BLIND.\n";
                    std::cout << "    TARGET COLOR CONFIG: When running the wizard setup, you MUST choose the correct target colors!\n";
                    std::cout << "    If you setup the filters to look for the wrong text color, the radar will not recognize any words.\n";
                    std::cout << "    FOREGROUND ACTIVE ONLY: This software ONLY runs when Elite Dangerous is active and in the foreground.\n";
                    std::cout << "    DO NOT open a stationary screenshot of the game window on your desktop and complain that the bot is not working!\n";

                    std::cout << "\n[3. PERFORMANCE OVERLOAD WARNING]\n";
                    std::cout << "    DO NOT overload the system! If you turn OFF Blue Circle Priority while simultaneously enabling BOTH\n";
                    std::cout << "    Gold Reward and White Name scanners, it will force full-screen sweeps and crawl at an extremely slow speed.\n";
                    std::cout << "    If your radar starts making a sharp, high-pitched telemetry noise, it means your FPS is lagging. Turn Blue Circle ON.\n";

                    std::cout << "\n[4. 100% BAN-SAFE / EULA GAME COMPLIANCE]\n";
                    std::cout << "    This utility does not read, write, modify, or inject anything into your Elite Dangerous game memory\n";
                    std::cout << "    or network packets. It is an independent screen-reading assistant that strictly abides by Frontier EULA.\n";
                    std::cout << "    It is 100% passive, completely undetectable, and absolutely Ban-Safe. Relax and use it safely.\n";

                    std::cout << "\n[5. CUSTOMER SUPPORT & CONTACT DISCLOSURE]\n";
                    std::cout << "    TECHNICAL SUPPORT EMAIL: jiahua960818@gmail.com\n";
                    std::cout << "    DEVELOPER NOTICE: I am a full-time student with an extremely heavy academic workload.\n";
                    std::cout << "                      If you run into bugs or issues, feel free to send me an email. However,\n";
                    std::cout << "                      it is completely normal and expected for me to take a few days to reply.\n";
                    std::cout << "                      Thank you very much for your understanding and patience!\n";

                    std::cout << "\n[6. SUPPORT THE DEVELOPER / BUY ME A CUP OF COFFEE]\n";
                    std::cout << "  If this  radar saved your endless grinding hours and successfully sniped your high-value cargo boards,\n";
                    std::cout << "  toss a few coins to buy the developer a cup of hot coffee!\n";
                    std::cout << "    https://ko-fi.com/fleogendepigge  \n";

                    std::cout << "========================================================================================================\n\n";
                }
                else if (cmd == "re") {
                    std::cout << "\n[RE-TRIGGER WIZARD] Rerunning Setup Wizard (FPS Configuration Skipped)...\n";

                    int backupDelay = g_PotatoScanDelayMs;

                    RunFullSetupWizard();

                    g_PotatoScanDelayMs = backupDelay;

                    std::cout << "[PRESET RELOADED] Successfully re-synchronized wizard parameters into memory!\n";
                }
                else if (cmd == "listw") {
                        std::cout << "\n[WHITE LIST] Current accepted targets:\n";
                        std::lock_guard<std::mutex> lock(g_ListMutex);
                        for (const auto& w : g_WhiteList) std::cout << " - " << w << "\n";
                        if (g_MinCreditReward > 0) std::cout << " - [CREDIT THRESHOLD] >= " << g_MinCreditReward << " CR\n";
                }
                else if (cmd == "listb") {
                            std::cout << "\n[BLACK LIST] Current filtered/ignored keywords:\n";
                            std::lock_guard<std::mutex> lock(g_ListMutex);
                            for (const auto& b : g_BlackList) std::cout << " - " << b << "\n";
                }
                else if (cmd == "addw") {
                                std::cout << "Enter NEW word to add into WHITE list: ";
                                std::string word; std::cin >> word;
                                transform(word.begin(), word.end(), word.begin(), ::toupper);

                                std::lock_guard<std::mutex> lock(g_ListMutex);
                                auto it = std::find(g_BlackList.begin(), g_BlackList.end(), word);
                                if (it != g_BlackList.end()) {
                                    std::cout << "\n[ACTION ABORTED] Operation failed! '" << word << "' already exists in the BLACK List.\n";
                                    std::cout << "   [WARNING] To avoid logic conflicts, you must manually delete it from Black List ([delb]) first!\n";
                                }
                                else {
                                    g_WhiteList.push_back(word);
                                    std::cout << "[SUCCESS] '" << word << "' added to White List successfully!\n";
                                    std::cout << "    [OCR ADV-NOTICE] Due to pixel-edge font distortion or text blur,\n";
                                    std::cout << "    OCR engine might easily misread letters! (e.g. 'O'->'0', 'I'->'1')\n";
                                    std::cout << "    Please also manually [addw] those 'short-sighted' variants to prevent missing tasks!\n";

                                    g_WasteImgFingerprints.clear();     g_WasteGoldFingerprints.clear();
                                    g_FullScanWhiteFingerprints.clear(); g_FullScanGoldFingerprints.clear();
                                }
                }
                else if (cmd == "addb") {
                                    std::cout << "Enter NEW word to add into BLACK list: ";
                                    std::string word; std::cin >> word;
                                    transform(word.begin(), word.end(), word.begin(), ::toupper);

                                    std::lock_guard<std::mutex> lock(g_ListMutex);
                                    auto it = std::find(g_WhiteList.begin(), g_WhiteList.end(), word);
                                    if (it != g_WhiteList.end()) {
                                        std::cout << "\n [ACTION ABORTED] Operation failed! '" << word << "' already exists in the WHITE List.\n";
                                        std::cout << "   [WARNING] To avoid logic conflicts, you must manually delete it from White List ([delw]) first!\n";
                                    }
                                    else {
                                        g_BlackList.push_back(word);
                                        std::cout << "[SUCCESS] '" << word << "' added to Black List successfully!\n";
                                        std::cout << "    [OCR ADV-NOTICE] Due to pixel-edge font distortion or text blur,\n";
                                        std::cout << "    OCR engine might easily misread letters! (e.g. 'O'->'0', 'I'->'1')\n";
                                        std::cout << "    Please also manually [addb] those 'short-sighted' variants to enforce strict filter!\n";

                                        g_WasteImgFingerprints.clear();     g_WasteGoldFingerprints.clear();
                                        g_FullScanWhiteFingerprints.clear(); g_FullScanGoldFingerprints.clear();
                                    }
                }
                else if (cmd == "delw") {
                    std::cout << "Enter EXACT word to remove from WHITE list: ";
                    std::string word; std::cin >> word;
                    transform(word.begin(), word.end(), word.begin(), ::toupper);

                    std::lock_guard<std::mutex> lock(g_ListMutex);
                    auto it = std::find(g_WhiteList.begin(), g_WhiteList.end(), word);
                    if (it != g_WhiteList.end()) {
                        g_WhiteList.erase(it);
                        std::cout << "[SUCCESS] Removed.\n";

                        g_WasteImgFingerprints.clear();     g_WasteGoldFingerprints.clear();
                        g_FullScanWhiteFingerprints.clear(); g_FullScanGoldFingerprints.clear();
                    }
                    else {
                        std::cout << "[ERROR] Word not found! '" << word << "' does not exist in White List.\n";
                    }
                }
                else if (cmd == "delb") {
                    std::cout << "Enter EXACT word to remove from BLACK list: ";
                    std::string word; std::cin >> word;
                    transform(word.begin(), word.end(), word.begin(), ::toupper);

                    std::lock_guard<std::mutex> lock(g_ListMutex);
                    auto it = std::find(g_BlackList.begin(), g_BlackList.end(), word);
                    if (it != g_BlackList.end()) {
                        g_BlackList.erase(it);
                        std::cout << "[SUCCESS] Removed.\n";

                        g_WasteImgFingerprints.clear();     g_WasteGoldFingerprints.clear();
                        g_FullScanWhiteFingerprints.clear(); g_FullScanGoldFingerprints.clear();
                    }
                    else {
                        std::cout << "[ERROR] Word not found! '" << word << "' does not exist in Black List.\n";
                    }
                    }
                else if (cmd == "stop") {
                        std::cout << "\n[TERMINATING] Shutting down optical radar engine cleanly...\n";
                        g_IsRunning = false; // Gracefully shut down main execution loops
                        g_CommandMode = false;

                        if (g_OcrApi) {
                            g_OcrApi->End();
                            delete g_OcrApi;
                            g_OcrApi = nullptr;
                        }
                        std::cout << "[OFFLINE] Goodbye, Commander. Program closed safely.\n";
                        return 0; // Terminate process cleanly, bypassing X window button dependencies
                        }
                else if (cmd == "/") {
                            std::cout << "[INFO] Command input canceled.\n";
                            }
                else {
                                std::cout << "[ERROR] Unknown command.\n";
                                }
                                g_CommandMode = false; // Unfreeze background scanning radar thread loop
                                std::cout << "[RADAR ACTIVE] Radar woke up, continuing full-speed sweep...\n\n";
            }
            else if (ch == 'h' || ch == 'H') {
                g_CurrentVolume += 5;
                if (g_CurrentVolume > 100) g_CurrentVolume = 100;
                ApplyHardwareVolume(g_CurrentVolume);
            }
            else if (ch == 'l' || ch == 'L') {
                g_CurrentVolume -= 5;
                if (g_CurrentVolume < 0) g_CurrentVolume = 0;
                ApplyHardwareVolume(g_CurrentVolume);
            }
        }

        if (g_CommandMode) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // B. Passive Radar Scanning Core (Only operates when Elite Dangerous is in foreground)
        if (eliteHwnd && GetForegroundWindow() == eliteHwnd) {

            bool isSideKeyActive = false;
            if (g_BindKeyUp == VK_XBUTTON1) {
                isSideKeyActive = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) || (GetAsyncKeyState(VK_XBUTTON2) & 0x8000);
            }
            bool isKbMouseScrolling = isSideKeyActive ||
                (GetAsyncKeyState(g_BindKeyUp) & 0x8000) ||
                (GetAsyncKeyState(g_BindKeyDown) & 0x8000) ||
                (GetAsyncKeyState(VK_DOWN) & 0x8000) ||
                (GetAsyncKeyState(VK_UP) & 0x8000);

            bool isGamepadScrolling = false;
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));
            if (XInputGetState(0, &state) == ERROR_SUCCESS) {
                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ||
                    (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)) {
                    isGamepadScrolling = true;
                }
            }

            // Dual-track sensor fusion
            bool isUserScrolling = isKbMouseScrolling || isGamepadScrolling;

            if (isUserScrolling) {

                static bool isGeometryLocked = false;
                if (!isGeometryLocked && eliteHwnd != NULL) {
                    RECT clientRect;
                    if (GetClientRect(eliteHwnd, &clientRect)) { 
                        int currentRealW = clientRect.right - clientRect.left;
                        int currentRealH = clientRect.bottom - clientRect.top;

                        g_TargetWidth = currentRealW;
                        g_TargetHeight = currentRealH;

                        double g_ScaleX = static_cast<double>(currentRealW) / 1920.0;
                        double g_ScaleY = static_cast<double>(currentRealH) / 1200.0;
                        g_Scale = (g_ScaleX + g_ScaleY) / 2.0;

                        std::cout << "\n[Calibration successful] Game client matched! Real-time bounds locked.\n";
                        std::cout << "  [INFO] Resolution: " << currentRealW << "x" << currentRealH << " | Factor g_Scale: " << g_Scale << "\n\n";

                        isGeometryLocked = true;
                    }
                }

                // 333ms Cardiac Telemetry Audio Pulse (With advanced continuous scroll check timer)
                static auto lastDebugBeepTime = std::chrono::steady_clock::now();
                auto currentDebugTime = std::chrono::steady_clock::now();
                auto elapsedDebugMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentDebugTime - lastDebugBeepTime).count();

                // State Machine Calibration: Tracks exactly when the user starts pressing keys
                static auto scrollStartTime = std::chrono::steady_clock::now();
                static bool wasScrollingLastFrame = false;

                if (isUserScrolling && !wasScrollingLastFrame) {
                    scrollStartTime = currentDebugTime; // Mark new scrolling onset timeline
                }
                wasScrollingLastFrame = isUserScrolling;

                // Live lookup register cache fed from the 1000ms sandbox below
                static int lastStableFPS = 11;

                if (elapsedDebugMs >= 333) {
                    if (!g_MutePulse) {
                        // Extract live active compression duration interval
                        auto continuousScrollMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentDebugTime - scrollStartTime).count();

                        // Continuous Hold Policy: Only morph beep tone when FPS is under 5 AND user has held scroll for over 1000ms!
                        // This fully neutralizes false overload alarms during rapid 1-click tap triggers!
                        if (lastStableFPS < 5 && isUserScrolling && continuousScrollMs >= 1000) {
                            // Heavy Hardware Overload / Lag Alert: Clean single high-frequency bleep
                            Beep(2500, 40); // Ultra-short 40ms 2500Hz burst. Pure tracking alert, zero delay penalty.
                        }
                        else {
                            // Nominal Scanning Flow: Comfort background pulse sound
                            Beep(450, 40);
                        }
                    }
                    lastDebugBeepTime = currentDebugTime;
                }

                // High-speed GDI/DWM BitBlt viewport clone (100% matched to client client-area pixels)
                if (eliteHwnd != NULL) {
                    cv::Mat fullFrame = GetGameScreenDynamic(eliteHwnd, 0, 0, g_TargetWidth, g_TargetHeight);
                    if (!fullFrame.empty()) {
                        if (ScanMissionsByExactGeometry(fullFrame)) {
                            PlayTargetDetectedAlertAsync();
                        }
                    }
                }

                // Accumulate valid full matrix scan throughput loop count
                fpsLoopCount++;

                // 1000ms Telemetry Sandbox: Locked update window for benchmarking performance
                auto currentFpsTime = std::chrono::steady_clock::now();
                auto elapsedFpsMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentFpsTime - lastFpsTime).count();
                if (elapsedFpsMs >= 1000) {
                    if (g_DebugMode) {
                        std::cout << "[PERF MONITOR] Radar Throughput: " << fpsLoopCount << " FPS (Effective Sweeps/Sec)\n";
                    }

                    // Critical Feedback Lock: Feed the final fixed FPS to the cardiac pulse check right before wiping registers!
                    lastStableFPS = fpsLoopCount;

                    fpsLoopCount = 0;             // Reset loop accumulation counter
                    lastFpsTime = currentFpsTime; // Shift stopwatch epoch
                }

                if (g_PotatoScanDelayMs > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(g_PotatoScanDelayMs));
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(11)); // Hardware breathing room, prevents thread spin burning
    }

    // Clean final garbage collection phase
    if (g_OcrApi) {
        g_OcrApi->End();
        delete g_OcrApi;
    }

    std::cout << "\n==================================================\n";
    std::cout << "[RADAR CRASH DETECTED] Press ANY key to close this diagnostics viewport...\n";
    system("pause");

    return 0;
}

void RunFullSetupWizard() {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "           ELITE ED SCANNER SETUP WIZARD         " << std::endl;
    std::cout << "==================================================" << std::endl;

    // ----------------------------------------------------------------
    //  STAGE 1: HARDWARE REFRESH RATE (FPS) CONFIGURATION
    // ----------------------------------------------------------------
    int sChoice = 0;
    do {
        std::cout << "\n[SETUP 1/4] Select hardware speed profile:\n";
        std::cout << "  1. Ludicrous Mode (45+ FPS - Maximum sweep speed for high-end gaming PC)\n";
        std::cout << "  2. Safe Mode      (25 FPS  - Balanced capture rate for standard multi-tasking)\n";
        std::cout << "  3. Potato PC Mode (10 FPS  - Ultra-low CPU snapshot overhead for legacy laptops)\n";
        std::cout << "  *NOTICE*: These values represent maximum CAP LIMITS enforced to protect your CPU from overheating.\n";
        std::cout << "            Actual performance depends heavily on your hardware specs AND your active scanning methods.\n";
        std::cout << "Select profile (1-3): ";
        std::cin >> sChoice;

        if (std::cin.fail()) {
            std::cin.clear(); std::cin.ignore(10000, '\n');
            sChoice = 0;
        }
        if (sChoice < 1 || sChoice > 3) std::cout << "[INVALID] Please enter 1, 2 or 3.\n";
    } while (sChoice < 1 || sChoice > 3);

    if (sChoice == 2) g_PotatoScanDelayMs = 30;
    else if (sChoice == 3) g_PotatoScanDelayMs = 90;
    else g_PotatoScanDelayMs = 0;
    std::cout << "[SUCCESS] Scan frequency updated.\n";

    // ----------------------------------------------------------------
    //  STAGE 2: RADAR SWEEP BAND SELECTION (DOUBLE-PASS FILTER)
    // ----------------------------------------------------------------
    int modeChoice = 0;
    do {
        std::cout << "\n[SETUP 2/4] Select radar operation sweep mode:\n";
        std::cout << "  1. BLUE CIRCLE PRIORITY : High-frequency lock (Scan only shareable tasks)\n";
        std::cout << "  2. GLOBAL TEXT SWEEP    : Full-screen scan (Thorough but extra CPU overhead)\n";
        std::cout << "Select sweep mode (1-2): ";
        std::cin >> modeChoice;

        if (std::cin.fail()) {
            std::cin.clear(); std::cin.ignore(10000, '\n');
            modeChoice = 0;
        }
        if (modeChoice < 1 || modeChoice > 2) std::cout << "[INVALID] Please enter 1 or 2.\n";
    } while (modeChoice < 1 || modeChoice > 2);

    if (modeChoice == 1) {
        g_OnlyBlueMissions = true;
        std::cout << "[SUCCESS] Radar armed with BLUE CIRCLE PRIORITY mode.\n";
    }
    else {
        g_OnlyBlueMissions = false;
        std::cout << "[SUCCESS] Radar armed with GLOBAL TEXT SWEEP mode.\n";
    }

// ----------------------------------------------------------------
//  STAGE 3: TARGET STRING SELECTION 
// ----------------------------------------------------------------
    int stringChoice = 0;
    do {
        std::cout << "\n[SETUP 3/4] Select target scanning layer text:\n";
        std::cout << "  1. Scan WHITE Text Only   (Mission Names)\n";
        std::cout << "  2. Scan GOLDEN Text Only  (Mission Rewards & Materials)\n";
        std::cout << "  3. Scan BOTH Layers       (WARNING: Doubles OCR workload! Heavy FPS performance drops!)\n";
        std::cout << "Select target layer (1-3): ";
        std::cin >> stringChoice;

        if (std::cin.fail()) {
            std::cin.clear(); std::cin.ignore(10000, '\n');
            stringChoice = 0;
        }
        if (stringChoice < 1 || stringChoice > 3) std::cout << "[INVALID] Please enter 1, 2 or 3.\n";
    } while (stringChoice < 1 || stringChoice > 3);
    if (stringChoice == 1) {
        g_ScanMissionName = true;   g_ScanMissionReward = false;
        std::cout << "[SUCCESS] Radar target locked onto WHITE layer only.\n";
    }
    else if (stringChoice == 2) {
        g_ScanMissionName = false;  g_ScanMissionReward = true;
        std::cout << "[SUCCESS] Radar target locked onto GOLDEN layer only.\n";
    }
    else {
        g_ScanMissionName = true;   g_ScanMissionReward = true;
        std::cout << "[NOTICE] Dual-layer scanning armed. Expect performance variations based on system load.\n";
    }

    // ----------------------------------------------------------------
    //  STAGE 4: QUICKPACK DYNAMIC PRESETS SWITCHING
    // ----------------------------------------------------------------
    int pkgChoice = 0;
    do {
        std::cout << "\n[SETUP 4/4] Select a Quick Presets Package to overwrite keyword lists:\n";
        std::cout << "  1. Keep Existing Local Settings (Load from saved config.txt)\n";
        std::cout << "  2. WMM Mission Pack             (Auto-scan Gold, Silver, Bertrandite, Indite)\n";
        std::cout << "  3. Rare Engineering Materials   (Auto-scan Biotech Conductors, Exquisite Focus, etc.)\n";
        std::cout << "  4. High-Value Commodity Trades  (Auto-scan Platinum, Painite, Opal, Diamonds)\n";
        std::cout << "  5. Pure Credits Reward Threshold (Trigger alert when payout is above certain amount)\n";
        std::cout << "  6. Pure Blank Master Lists      (Wipe everything, start with 100% EMPTY list)\n";
        std::cout << "Select package preset (1-6): ";
        std::cin >> pkgChoice;

        if (std::cin.fail()) {
            std::cin.clear(); std::cin.ignore(10000, '\n');
            pkgChoice = 0;
        }
        if (pkgChoice < 1 || pkgChoice > 6) std::cout << "[INVALID] Please enter 1 to 6.\n";
    } while (pkgChoice < 1 || pkgChoice > 6);

    {
        std::lock_guard<std::mutex> lock(g_ListMutex);
        if (pkgChoice == 1) { std::cout << "[INFO] Maintained current memory registers.\n"; }
        else if (pkgChoice == 2) { g_WhiteList = PKG_WMM_WHITE; g_BlackList = PKG_WMM_BLACK; g_MinCreditReward = 0; }
        else if (pkgChoice == 3) { g_WhiteList = PKG_RARE_MATS; g_BlackList.clear(); g_MinCreditReward = 0; }
        else if (pkgChoice == 4) { g_WhiteList = PKG_RARE_GOODS; g_BlackList.clear(); g_MinCreditReward = 0; }
        else if (pkgChoice == 5) {
            long long tempReward = -1;
            do {
                std::cout << "Enter the EXACT minimum Credits reward (e.g., enter 20000000 for 20 Million): ";
                std::cin >> tempReward;
                if (std::cin.fail() || tempReward < 0) {
                    std::cin.clear(); std::cin.ignore(10000, '\n');
                    tempReward = -1;
                    std::cout << "[ERROR] Invalid amount! Please enter positive full digits.\n";
                }
            } while (tempReward < 0);
            g_MinCreditReward = tempReward;
            g_WhiteList.clear(); g_BlackList.clear();
        }
        else if (pkgChoice == 6) {
            g_WhiteList.clear(); g_BlackList.clear(); g_MinCreditReward = 0;
            std::cout << "[PURE BLANK] Master lists have been completely wiped!\n";
        }
        g_WasteImgFingerprints.clear();     g_WasteGoldFingerprints.clear();
        g_FullScanWhiteFingerprints.clear(); g_FullScanGoldFingerprints.clear();
    }
    SaveConfigToDisk();
    std::cout << "\n[CONFIG SAVED] All setup configurations permanently written to '.\\config.txt'!\n";
    std::cout << "==================================================\n\n";

    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

}

