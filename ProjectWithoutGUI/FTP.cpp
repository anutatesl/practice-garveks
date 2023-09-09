#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include "FTP.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <algorithm>

#include <experimental/filesystem>

const char APPROVAL = 'y';
const char REJECTION = 'n';

namespace fs = std::experimental::filesystem; //The contents of <filesystem> are available only with C++17 or later.

bool FTP::openConnection(const std::string& portName) {
    int baudrate = 9600;
    if (port.openPort(portName, baudrate)) {
        std::cout << "Port (" << portName << ") was opened\n";
        return true;
    }
    else {
        std::cout << "Port (" << portName << ") was not opened\n";
        return false;
    }
}

void FTP::closeConnection() {
    port.closePort();
    std::cout << "The connection is closed.\n";
}

void FTP::sendFile(const std::string& portName, const std::string& file) {

    if (!openConnection(portName)) {
        std::cout << "Couldn't recieve a file\n";
        return;
    }

    DWORD dwSize;
    char elem = ' ';
    unsigned long read = 0;
    std::ifstream in;
    in.open(file, std::ifstream::ate | std::ifstream::binary);
    if (!in.is_open()) {
        std::cout << "File not found\n";
        closeConnection();
        return;
    }
    unsigned long file_size = in.tellg();
    std::cout << "Rope Weight : " << file_size << " byte" << "\n";
    std::string s_file_size = std::to_string(file_size);
    port.writeData(s_file_size.c_str(), s_file_size.length());
    if (!getAnswer(elem, read)) {
        std::cout << "Permission was not received\n";
        closeConnection();
        return;
    }

    in.seekg(0, in.beg);
    std::vector<char> filebuffer;
    filebuffer.reserve(file_size);
    char c_tmp;
    for (auto i = 0; i < file_size; i++)
    {
        in.read(&c_tmp, sizeof(c_tmp));
        filebuffer.push_back(c_tmp);
    }
    in.close();
    unsigned long check_sum = calculateChecksumCRC32(filebuffer, file_size);
    std::cout << "CRC32 = " << check_sum << "\n";
    std::string s_check_sum = std::to_string(check_sum);
    port.writeData(s_check_sum.c_str(), s_check_sum.length());
    std::cout << "Requesting permission to transfer data\n";
    if (!getAnswer(elem, read)) {
        std::cout << "Permission was not received\n";
        closeConnection();
        return;
    }

    std::cout << "Start sent data\n";
    const int fragment_size = 10;
    int size = 0;
    int count_error = 0;
    bool correct_fragment;
    int indFilebuffer = 0;
    int trialSize = filebuffer.size();
    while (trialSize > 0 && count_error != MAX_ERROR) {
        size = (trialSize < fragment_size) ? trialSize : fragment_size;

        std::vector<char> mass_fragment;
        for (int indFragment = 0; indFragment < size; indFragment++)
            mass_fragment.push_back(filebuffer[indFragment + indFilebuffer]);
        indFilebuffer += size;
        trialSize -= fragment_size;
        correct_fragment = false;
        count_error = 0;
        while (!correct_fragment && count_error != MAX_ERROR) {
            count_error++;
            port.writeData(mass_fragment.data(), size);
            if (!getAnswer(elem, read)) {
                std::cout << "Fragment was not recieved\n";
                closeConnection();
                return;
            }
            unsigned short check_sum = calculateChecksumCRC16(mass_fragment, size);
            std::cout << "CRC16 = " << check_sum << "   ";
            std::string s_check_sum = std::to_string(check_sum);
            port.writeData(s_check_sum.c_str(), s_check_sum.length());
            if (getAnswer(elem, read)) {
                correct_fragment = true;
                std::cout << "The fragment was sent and received correctly\n";
            }
            else {
                correct_fragment = false;
                std::cout << "The fragment was sent and received incorrectly, I try again\n";
            }
        }
        mass_fragment.clear();
    }
    if (count_error == MAX_ERROR) {
        std::cout << "Error! The file was sent and received incorrectly\n";
        closeConnection();
        return;
    }
    if (getAnswer(elem, read)) {
        std::cout << "The file was successfully accepted sent and accepted by the second part\n";
    }
    else {
        std::cout << "Error! The file was sent and received incorrectly\n";
    }
    closeConnection();   
}

bool FTP::receiveFile(const std::string& portName, const std::string& folderPath, const std::string& fileName) {
    if (!openConnection(portName)) {
        std::cout << "Couldn't receive a file\n";
        return false;;
    }

    std::vector<char> dst; 
    dst.resize(fragmentWeight);
    unsigned long read = 0, bytes = 0;
    std::size_t fullSize = 0;
    int counter = 0;
    unsigned long fullChecksum = 0, trialChecksum = 0, checksum = 0;
    std::vector<char> buffer;
    bool successFlag = true;
    DWORD dwSize = sizeof(APPROVAL);

    port.readData(dst.data(), read);
    fullSize = std::atoi(dst.data());
    if (fullSize <= 0) {
        std::cout << "Uncorrect size\n";
        port.writeData(&REJECTION, dwSize);
        return false;
    }
    else 
        std::cout << "Accepted size\n";
    port.writeData(&APPROVAL, dwSize);

    port.readData(dst.data(), read);
    fullChecksum = std::atoll(dst.data());
    std::cout << "Accepted checksum\n";


    port.writeData(&APPROVAL, dwSize);
    std::cout << "Ready to accept data\n";

    std::vector<char> dstSum;
    dstSum.resize(fragmentWeight);
    while (true) {
        if (counter == MAX_ERROR) {
            std::cout << "Too much tries for one fragment!\n";
            closeConnection();
            successFlag = false;
            break;
        }
        port.readData(dst.data(), read); //fragment
        dst.erase(dst.begin() + read, dst.end());
        bytes += read;
        trialChecksum = calculateChecksumCRC16(dst, read);
        port.writeData(&APPROVAL, dwSize);
        std::cout << "Accepted fragment\n";

        dstSum.clear();
        dstSum.resize(fragmentWeight);
        port.readData(dstSum.data(), read); // checksum
        checksum = std::atol(dstSum.data());
       
        std::cout << "Accepted checksum\n";
        if (trialChecksum != checksum) {
            std::cout << "The fragment was not accepted successfully! Try again\n";
            port.writeData(&REJECTION, dwSize);
            counter++;
            continue;
        }
        else {
            std::cout << "The fragment was accepted successfully\n";
            port.writeData(&APPROVAL, dwSize);
            buffer.insert(buffer.end(), dst.begin(), dst.end());
            trialChecksum = calculateChecksumCRC32(buffer, buffer.size());
            counter = 0;
        }
        if (fullSize == bytes && fullChecksum == trialChecksum) {
            std::cout << "The file was accepted successfully!\n";
            port.writeData(&APPROVAL, dwSize);
            successFlag = true;
            closeConnection();
            break;
        }
    }

    if (successFlag) {
        //write file into the folder
        std::error_code e;
        fs::path dir = folderPath;
        bool flag = fs::create_directories(dir, e);
        fs::current_path(dir);
        FILE* fp;
        if ((fopen_s(&fp, fileName.c_str(), "w")) == 0) {  //wb+
            for (int i = 0; i < buffer.size(); i++) {
                fputc(buffer[i], fp);
                if (feof(fp)) {
                    std::cout << "Couldn't write to the new file\n";
                    break;
                }
            }
            if (!feof(fp)) 
                std::cout << "File was written to the folder\n";
            fclose(fp);
        }
        else {
            std::cout << "Couldn't write to the new file\n";
        }
        return true;
    }
    else {
        std::cout << "The file was not accepted!\n";
        return false;
    }
}

/*We describe the Crc32 calculation function
 using a polynomial EDB88320UL=
 x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11
 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1*/
unsigned long FTP::calculateChecksumCRC32(std::vector<char>& buffer, unsigned long count) {//(unsigned char* mass, unsigned long count) {
    //initialize the Crc32 calculation table
    unsigned char* mass = new unsigned char[count];
    for (int i = 0; i < count; i++) {
        mass[i] = buffer[i];
    }

    unsigned long crc_table[256];//mass 32 bit = 4 byte
    unsigned long crc;//variable 32 bit = 4 byte
    for (int i = 0; i < 256; i++)//initialize the array loop
    {
        crc = i;
        for (int j = 0; j < 8; j++)//polynomial iteration cycle
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
        crc_table[i] = crc;
    };
    crc = 0xFFFFFFFFUL;
    while (count--)// checking the continuation condition
        crc = crc_table[(crc ^ *mass++) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFUL;
}

/*We describe the Crc16 standard CCITT calculation function
using the polynomial 1021=x^16+x^12 +x^5+1*/
unsigned short FTP::calculateChecksumCRC16(std::vector<char>& buffer, unsigned long count) {
    unsigned char* mass = new unsigned char[count];
    for (int i = 0; i < count; i++) {
        mass[i] = buffer[i];
    }

    unsigned short crc = 0xFFFF;//variable 16 bit = 2 byte
    unsigned char i; //variable 8 bit = 1 byte
    while (count--)// checking the continuation condition
    {
        crc ^= *mass++ << 8;
        for (i = 0; i < 8; i++)//polynomial iteration cycle
            crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

bool FTP::getAnswer(char elem, unsigned long& read) {
    bool flag = false;
    port.readData(&elem, read);
    if (elem == APPROVAL) {
        flag = true;
    }
    return flag;
}