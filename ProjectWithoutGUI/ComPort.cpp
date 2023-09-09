#include "ComPort.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>

ComPort::ComPort() {
    cPort = INVALID_HANDLE_VALUE;
}

ComPort::~ComPort() {
    closePort();
}

bool ComPort::openPort(const std::string& port, const int& baudrate) {

    closePort();
    wchar_t* wPort = convertToLPCTSTR(port);
    LPCTSTR portName = wPort; 
    cPort = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (cPort == INVALID_HANDLE_VALUE) {
        std::cout << "Comm Port was not open successfully\n";
        DWORD dw = GetLastError();
        std::cout << std::to_string(dw) << "\n";
        CloseHandle(cPort);
        cPort = INVALID_HANDLE_VALUE;
        return false;
    }
    else {
        if (installPortSettings(baudrate) && installPortTimeouts()) {
            std::cout << "Comm Port will be opened successfully\n";
            return true;
        }
        else {
            CloseHandle(cPort);
            cPort = INVALID_HANDLE_VALUE;
            std::cout << "Comm Port was not open successfully\n";
            return false;
        }
    }
}

bool ComPort::installPortSettings(const int& baudrate) {
    DCB PortDCB = { 0 };
    PortDCB.DCBlength = sizeof(DCB); // getting the default information of the DCB structure
    if (!GetCommState(cPort, &PortDCB)) {
        std::cout << "GetCommState failed with error %d.\n " << std::to_string(GetLastError());
        return false;
    }
    // initialization of port parameters by setting values of DCB structure fields
    PortDCB.BaudRate = DWORD(baudrate); // data exchange rate 9600 baud/s  
    PortDCB.fBinary = TRUE;  // binary exchange mode
    PortDCB.ByteSize = 8;
    PortDCB.Parity = NOPARITY;  // 0-4 = no,odd,even,mark,space
    PortDCB.StopBits = ONESTOPBIT;

    if (!SetCommState(cPort, &PortDCB)) {
        std::cout << "Unable to configure the serial port\n";
        std::cout << "SetCommState failed with error %d.\n " << std::to_string(GetLastError());
        return false;
    }
    return true;
}

bool ComPort::installPortTimeouts() {
    COMMTIMEOUTS CommTimeouts;
    if (!GetCommTimeouts(cPort, &CommTimeouts)) {
        std::cout << "GetCommTimeouts failed with error %d.\n " << std::to_string(GetLastError());
        return false;
    }
    CommTimeouts.ReadIntervalTimeout = 0xFFFFFFFF; // maximum time between reading two characters
    CommTimeouts.ReadTotalTimeoutMultiplier = 0;
    CommTimeouts.ReadTotalTimeoutConstant = 0;
    CommTimeouts.WriteTotalTimeoutMultiplier = 0;
    CommTimeouts.WriteTotalTimeoutConstant = 1500; // no delay when reading

    if (!SetCommTimeouts(cPort, &CommTimeouts))
    {
        std::cout << "Unable to set the timeout parameters\n";
        std::cout << "SetCommTimeouts failed with error %d.\n " << std::to_string(GetLastError());
        return false;
    }
    return true;
}

bool ComPort::closePort() {
    if (cPort != INVALID_HANDLE_VALUE) {
        CloseHandle(cPort);
        cPort = INVALID_HANDLE_VALUE;
        std::cout << "Comm Port will be closed successfully\n";
        return true;
    }
    return false;
}

bool ComPort::writeData(LPCVOID data, const DWORD& dwSize) {
    DWORD dwBytesWritten;
    BOOL iRet = WriteFile(cPort, data, dwSize, &dwBytesWritten, NULL);
    if (!iRet || dwBytesWritten != dwSize) {
        CloseHandle(cPort);
        cPort = INVALID_HANDLE_VALUE;
        std::cout << "Error writing file size to port\n";
        return false;
    }
    return true;
}

bool ComPort::readData(LPVOID dst, unsigned long& read) {
    const int READ_TIME = 100;
    OVERLAPPED sync = { 0 };
    int result = 0;
    unsigned long size = 1024;
    unsigned long wait = 0, state = 0;
    sync.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // Creating a synchronization object

    if (SetCommMask(cPort, EV_RXCHAR)) {  // Setting the mask for port events  
        WaitCommEvent(cPort, &state, &sync);  // Linking the port and the synchronization object   
        wait = WaitForSingleObject(sync.hEvent, READ_TIME);  // Start waiting for data
        if (wait == WAIT_OBJECT_0) {  // Data received
            ReadFile(cPort, dst, size, &read, &sync);
            wait = WaitForSingleObject(sync.hEvent, READ_TIME);
            if (wait == WAIT_OBJECT_0) {
                if (GetOverlappedResult(cPort, &sync, &read, FALSE)) {
                    result = read;
                    /*----------*/
                    if (result != 0) {
                        CloseHandle(sync.hEvent);
                        return true;
                    }
                    else {
                        if (readData(dst, read)) {
                            return true;
                        }
                        else {
                            return false;
                        }
                    }
                    /*----------*/
                }
            }
        }
    }
    CloseHandle(sync.hEvent);
    return true;
}

wchar_t* ComPort::convertToLPCTSTR(const std::string& str) {
    char* chars = new char[str.length() + 1];
    str.copy(chars, str.length());
    chars[str.length()] = '\0';

    size_t size = strlen(chars) + 1;
    wchar_t* wchars = new wchar_t[size];
    size_t outSize;
    mbstowcs_s(&outSize, wchars, size, chars, size - 1);

    return wchars;
}