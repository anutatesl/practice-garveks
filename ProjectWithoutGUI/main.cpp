#include <iostream>
#include "FTP.h"

int main() {
	FTP obj;
	const std::string portName = "COM6";
	const std::string folderPath = "";
	const std::string fileName = "";

	obj.sendFile(portName, fileName);
	//obj.receiveFile(portName, folderPath, fileName);
}
