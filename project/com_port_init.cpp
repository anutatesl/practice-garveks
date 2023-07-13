#include "COM_PORT_INIT.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>

Com_port::Com_port() {
    cPort = INVALID_HANDLE_VALUE;
}

Com_port::~Com_port() {
    closePort();
}

bool Com_port::openPort(const std::string& port, int baudrate) {
    
    closePort();
    LPCTSTR portName = L"COM1";
    //(LPCTSTR)port.c_str() error code 123 Incorrect syntax of the file name, directory name, or volume label.
    cPort = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); // 0 instead of FILE_ATTRIBUTE_NORMAL means there will be synchronous transmission
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

bool Com_port::installPortSettings(int baudrate) {
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

bool Com_port::installPortTimeouts() {
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

void Com_port::closePort() {
    if (cPort != INVALID_HANDLE_VALUE) {
        CloseHandle(cPort);
        cPort = INVALID_HANDLE_VALUE;
        std::cout << "Comm Port will be closed successfully\n";
    }
}

void Com_port::readData() {
    int counter = 0;
    std::vector<char> buffer;
    int fullSize = 0;
    long int fullChecksum = 0;
    long int checksum = 0;
    long int trialChecksum = 0;
    const int READ_TIME = 100;
    OVERLAPPED sync = { 0 };
    int result = 0;
    unsigned long wait = 0, read = 0, state = 0;
    char dst[1024] = { 0 }; //unsigned
    unsigned long size = sizeof(dst);
    // ������� ������ ������������� 
    sync.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // ������������� ����� �� ������� ����� 
    if (SetCommMask(cPort, EV_RXCHAR)) {
        // ��������� ���� � ������ ������������� 
        WaitCommEvent(cPort, &state, &sync);
        // �������� �������� ������ 
        wait = WaitForSingleObject(sync.hEvent, READ_TIME);
        // ������ ��������  
        if (wait == WAIT_OBJECT_0) {
            ReadFile(cPort, dst, size, &read, &sync); //size of file
            fullSize = std::atoi(dst);
            //fullSize = std::stoi(std::string(buffer));
            if (fullSize <= 0) {
                std::cout << "Uncorrect size\n";
            }
            memset(dst, 0, 1024);
            wait = 0;
            sync.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            wait = WaitForSingleObject(sync.hEvent, READ_TIME);
            if (wait == WAIT_OBJECT_0) {
                ReadFile(cPort, dst, size, &read, &sync); //full file check sum
                fullChecksum = std::atoi(dst);
                if (fullChecksum <= 0) {
                    std::cout << "Uncorrect checksum\n";
                }
                memset(dst, 0, 1024);
                //���� ����� � ���������� ������� ������
                while (trialChecksum != fullChecksum) {
                    if (counter == MAX_ERROR) {
                        std::cout << "Too much tries for one fragment!\n";
                        //close
                        break;
                    }
                    sync.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                    wait = WaitForSingleObject(sync.hEvent, READ_TIME);
                    if (wait == WAIT_OBJECT_0) {
                        ReadFile(cPort, dst, size, &read, &sync); //fragment    
                        sync.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                        wait = WaitForSingleObject(sync.hEvent, READ_TIME);
                        if (wait == WAIT_OBJECT_0) {
                            ReadFile(cPort, dst, size, &read, &sync); // checksum
                            checksum = std::atoi(dst);
                            //memset(dst, 0, 1024);
                        }
                        if (calculateChecksum(dst, size) != checksum) {
                            //uncorrect fragment
                            std::cout << "The fragment was not accepted successfully! Try again\n";
                            counter++;
                            continue;
                        }
                        else {
                            std::cout << "The fragment was accepted successfully\n";
                            for (int i : dst) {
                                buffer.push_back(dst[i]);
                            }
                            counter = 0;
                        }
                    }
                    memset(dst, 0, 1024);
                    int buffSize = buffer.size();
                    char* array = new char[buffSize];
                    for (int i = 0; i < buffSize; i++) {
                        array[i] = buffer[i];
                    }
                    trialChecksum = calculateChecksum(array, buffSize);
                    delete[] array;
                    if (trialChecksum > fullChecksum) {
                        //close
                        std::cout << "Too much fragments\n";
                        break;
                    }
                    /*
                    if (wait == WAIT_OBJECT_0)
                        if (GetOverlappedResult(cPort, &sync, &read, FALSE)) {
                            result = read;
                            std::cout << "Size of fragment: " << result << '\n';
                        }
                        */
                }
                if (trialChecksum == fullChecksum) {
                    //write file into the folder
                    std::cout << "The file was accepted successfully!\n";
                    //close
                }
            }
        }
    }
    //std::cout << "Received bytes: " << result;
    //std::cout << "\nFile: " << dst;
    CloseHandle(sync.hEvent);
   //DWORD iSize; //������ ���������� ��� �������� ���������� �� ���������� ����������� ���� (� ������ ��������� ������)
   //char sReceivedChar[2]; //������ ���������� ���� ����� �������� ����������� ������
   //char recBuf[100];
   //std::string Symb;
   //sReceivedChar[1] = 0;
   //do {
   //    //ReadFile(cPort, &sReceivedChar, 1, &iSize, 0); 	// �������� 1 ����
   //    // ���� ���-�� �������, �������
   //    if (iSize > 0) {
   //        strcat(recBuf, sReceivedChar);
   //    }
   //} while (iSize > 0);
    //DWORD iSize;
    //char sReceivedChar;
    //while (true)
    //{
    //    ReadFile(cPort, &sReceivedChar, 1, &iSize, 0);  // �������� 1 ����
    //    if (iSize > 0)   // ���� ���-�� �������, �������
    //        std::cout << sReceivedChar;
    //}
}

void Com_port::writeData(const std::string& file) {
    
    if (cPort == INVALID_HANDLE_VALUE) {
        //
    }

    std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
    unsigned long file_size =  in.tellg();
    std::cout << "\nRope Weight : " << file_size << " byte" << "\n";

    FILE* read_all_file;
    fopen_s(&read_all_file, "hello.txt", "r"); //!!!!!!!!!!!!
    int all_file_size = file_size;
    char* mass_all_file = new char[all_file_size];
    if (read_all_file) {
        unsigned long count = fread(mass_all_file, sizeof mass_all_file[0], file_size, read_all_file);
        //printf("read %zu elements out of %d\n", count, file_size);
        //printf(mass_all_file);
        fclose(read_all_file);
        std::cout << "\n";
        
        unsigned long check_sum = calculateChecksumCRC32(mass_all_file, count);
        std::cout << "CRC = " << check_sum << "\n";

        //std::ofstream out("res.txt");
        //for (int i = 0; i < count; i++) {
        //    out << mass_all_file[i];
        //}
        //out.close();
        //std::ifstream in("res.txt", std::ifstream::ate | std::ifstream::binary);
        //unsigned long int file_size = in.tellg();
        //std::cout << "\nRope Weight : " << file_size << " byte" << "\n";

    } else {
        std::cout << "File not found\n";
    }

    DWORD dwBytesWritten;
    unsigned long int buff = file_size;
    FILE* fp;
    fopen_s(&fp, "hello.txt", "r");
    const int fragment_size = 10;
    char mass_fragment[fragment_size];
    if (fp) {
        while (buff > fragment_size) {
            size_t count = fread(mass_fragment, sizeof mass_fragment[0], fragment_size, fp);
            buff -= fragment_size;

            DWORD dwSize = sizeof(mass_fragment);
            BOOL iRet = WriteFile(cPort, &mass_fragment, dwSize, &dwBytesWritten, NULL);
            if (dwBytesWritten != dwSize) {
                CloseHandle(cPort);
                cPort = INVALID_HANDLE_VALUE;
                std::cout << "Error writing to port\n";
            }
        }
        int endPart_fragment_size = buff;
        char* endPart_mass_fragment = new char[endPart_fragment_size];
        if (buff > 0) {
            size_t count = fread(endPart_mass_fragment, sizeof endPart_mass_fragment[0], buff, fp);
            //endPart_mass_fragment[buff - 1] = '\n';
            printf("read %zu elements out of %d\n", count, buff);
            //printf(endPart_mass_fragment);
        }
        DWORD dwSize = sizeof(endPart_mass_fragment);
        BOOL iRet = WriteFile(cPort, &endPart_mass_fragment, dwSize, &dwBytesWritten, NULL);
        if (dwBytesWritten != dwSize) {
            CloseHandle(cPort);
            cPort = INVALID_HANDLE_VALUE;
            std::cout << "Error writing to port\n";
        }
        std::cout << dwSize << " Bytes in string. " << dwBytesWritten << " Bytes sended.\n\n";

        delete[] endPart_mass_fragment;
        delete[] mass_all_file;
        fclose(fp);
    }
    else {
        std::cout << "File not found\n";
    }
}

unsigned long Com_port::calculateChecksumCRC32(char* mass, unsigned long count) {
    //�������������� ������� ������� Crc32
    unsigned long crc_table[256];//������ 32 ���� = 4 ������
    unsigned long crc;//���������� 32 ���� = 4 ������
    for (int i = 0; i < 256; i++)//�������������� ���� �������
    {
        crc = i;
        for (int j = 0; j < 8; j++)//���� �������� ��������
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
        crc_table[i] = crc;
    };
    crc = 0xFFFFFFFFUL;
    while (count--)// �������� ������� �����������
        crc = crc_table[(crc ^ *mass++) & 0xFF] ^ (crc >> 8);
    return crc^ 0xFFFFFFFFUL; //����� ������� ������� Crc32
}