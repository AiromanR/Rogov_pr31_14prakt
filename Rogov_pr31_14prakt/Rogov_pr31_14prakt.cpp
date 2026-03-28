#include <iostream>
#include <windows.h>
#include <ctime>

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4

struct ClientRecord {
    DWORD threadId;
    DWORD arriveTick;
    DWORD startTick;
    DWORD endTick;
    BOOL served;
    BOOL timeout;
};

struct ClubState {
    ClientRecord clients[MAX_CLIENTS];
    LONG currentVisitors;
    LONG maxVisitors;
    LONG servedCount;
    LONG timeoutCount;
};

ClubState clubstate;
HANDLE Semaphores = NULL;
bool allDone = false;

DWORD WINAPI ClientThread(LPVOID id) {
    int ID = (int)id;
    clubstate.clients[ID].arriveTick = GetTickCount64();

    DWORD waitResult = WaitForSingleObject(Semaphores, 3000);
    if (waitResult == WAIT_OBJECT_0) {
        clubstate.currentVisitors++;
        clubstate.clients[ID].startTick = GetTickCount64();
        Sleep(2000 + (rand() % 3000));
        clubstate.currentVisitors--;
        clubstate.clients[ID].endTick = GetTickCount64();
        clubstate.clients[ID].served = TRUE;
        clubstate.servedCount++;
        ReleaseSemaphore(Semaphores, 1, NULL);
    }
    else {
        clubstate.clients[ID].endTick = GetTickCount64();
        clubstate.clients[ID].timeout = TRUE;
        clubstate.timeoutCount++;
    }
    return 0;
}


DWORD WINAPI ObserverThread(LPVOID lpParam) {
    while (!allDone) {
        Sleep(500);
        std::cout << "Занято: " << clubstate.currentVisitors
                  << " | Обслужено: " << clubstate.servedCount
                  << " | Таймаутов: " << clubstate.timeoutCount 
                  << std::endl;
    }
    std::cout << "\nСтатистика\n";
    std::cout << "Максимальное количество одновременно занятых мест: " << clubstate.maxVisitors << std::endl;
    std::cout << "Всего обслужено посетителей: " << clubstate.servedCount << std::endl;
    std::cout << "Ушли по таймауту: " << clubstate.timeoutCount << std::endl;
    if (clubstate.servedCount > 0) {
        DWORD totalWait = 0, totalService = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clubstate.clients[i].served) {
                totalWait += (clubstate.clients[i].startTick - clubstate.clients[i].arriveTick);
                totalService += (clubstate.clients[i].endTick - clubstate.clients[i].startTick);
            }
        }
        std::cout << "Среднее время ожидания: " << (totalWait / clubstate.servedCount) << " мс" << std::endl;
        std::cout << "Среднее время обслуживания: " << (totalService / clubstate.servedCount) << " мс" << std::endl;
    }
    std::cout << "\nСписок посетителей, которые не дождались места:\n";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clubstate.clients[i].timeout) {
            std::cout << "Поток " << clubstate.clients[i].threadId << std::endl;
        }
    }
    return 0;
}

int main() {
    setlocale(0, "rus");
    srand(time(NULL));

    if (!(Semaphores = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, NULL))) 
        return GetLastError();
    
    HANDLE hObserver;
    if (hObserver = CreateThread(NULL, 0, ObserverThread, NULL, 0, NULL)) {
        SetThreadPriority(hObserver, THREAD_PRIORITY_LOWEST);
    } else return GetLastError();

    HANDLE hVisitors[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        DWORD tid;
        int priority;

        if (hVisitors[i] = CreateThread(NULL, 0, ClientThread, (LPVOID)i, 0, &tid)) {
            clubstate.clients[i].threadId = tid;
            priority = THREAD_PRIORITY_NORMAL;

            if (i >= 16) priority = THREAD_PRIORITY_HIGHEST;
            else if (i >= 8) priority = THREAD_PRIORITY_BELOW_NORMAL;
            SetThreadPriority(hVisitors[i], priority);
        } else return GetLastError();
    }

    std::cout << "Клуб открыт, " << MAX_CLIENTS << " посетителей\n" << std::endl;

    WaitForMultipleObjects(MAX_CLIENTS, hVisitors, TRUE, INFINITE);
    allDone = true;

    WaitForSingleObject(hObserver, INFINITE);
    CloseHandle(hObserver);
    for (int i = 0; i < MAX_CLIENTS; i++) CloseHandle(hVisitors[i]);
    CloseHandle(Semaphores);

    return 0;
}