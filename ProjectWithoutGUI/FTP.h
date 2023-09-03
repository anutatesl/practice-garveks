#ifndef FILE_TRANSFER_PROTOCOL
#define FILE_TRANSFER_PROTOCOL

#include "ComPort.h"
#include <vector>

class FTP //file transfer protocol
{
public:
	bool openConnection(const std::string& portName);
	void closeConnection();

	virtual void sendFile(const std::string& portName, const std::string& file);
	virtual bool receiveFile(const std::string& portName, const std::string& folderPath, const std::string& fileName);

private:
	ComPort port;
	const int MAX_ERROR = 3;

	unsigned long calculateChecksumCRC32(std::vector<char>& buffer, unsigned long count);
	unsigned short calculateChecksumCRC16(char* mass, unsigned long count);

	bool getAnswer(char* elem, unsigned long& read);
};

#endif