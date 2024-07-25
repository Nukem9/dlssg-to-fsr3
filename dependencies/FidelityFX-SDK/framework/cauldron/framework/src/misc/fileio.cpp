// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "misc/fileio.h"
#include "misc/assert.h"

#include <algorithm>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WINDOWS)
    #include <io.h>
    #define S_ISREG(e) (((e) & _S_IFMT) == _S_IFREG)
    #define S_ISDIR(e) (((e) & _S_IFMT) == _S_IFDIR)
#else
    #error Platform needs to implement FileI/O
#endif

namespace cauldron
{
    int64_t ReadFileAll(const wchar_t* fileName, void* buffer, size_t bufferLen)
    {
        // Try to open the file
        int file = -1;
        (void)_wsopen_s(&file, fileName, _O_RDONLY | _O_NOINHERIT | _O_BINARY | _O_SEQUENTIAL, _SH_DENYNO, _S_IREAD);

        // Unable to open file
        if (file == -1)
            return -1;

        // Get the file size
        struct _stat64 fileStatus;
        if (_fstat64(file, &fileStatus) != 0)
        {
            // Unable to get size
            (void)_close(file);
            return -1;
        }

        // Check if file is valid
        if (!S_ISREG(fileStatus.st_mode))
        {
            // Not a regular file
            (void)_close(file);
            return -1;
        }

        const uint64_t fileLengthBytes = static_cast<uint64_t>(std::max<int64_t>(fileStatus.st_size, 0));
        if (fileLengthBytes > bufferLen)
            return -1;

        // Fill in the buffer
        char* pFileBuffer = (char*)buffer;
        for (uint64_t bytesLeft = fileLengthBytes; bytesLeft > 0;)
        {
            uint32_t bytesRequested = static_cast<uint32_t>(std::min<uint64_t>(bytesLeft, INT32_MAX));
            auto bytesReceivedOrStatus = _read(file, pFileBuffer, bytesRequested);

            // Break into 4GB chunks
            if (bytesReceivedOrStatus > 0)
            {
                bytesLeft -= bytesReceivedOrStatus;
                pFileBuffer += bytesReceivedOrStatus;
            }

            // Going past eof... not good
            else if (bytesReceivedOrStatus == 0)
            {
                (void)_close(file);
                return -1;
            }

            // Error reported during read
            else if (bytesReceivedOrStatus < 0)
            {
                (void)_close(file);
                return -1;
            }
        }

        // All done
        (void)_close(file);

        return fileLengthBytes;
    }

    int64_t ReadFilePartial(const wchar_t* fileName, void* buffer, size_t bufferLen, int64_t readOffset/*= 0*/)
    {
        // Try to open the file
        int file = -1;
        (void)_wsopen_s(&file, fileName, _O_RDONLY | _O_NOINHERIT | _O_BINARY | _O_SEQUENTIAL, _SH_DENYNO, _S_IREAD);

        // Unable to open file
        if (file == -1)
            return -1;

        // Get the file size
        struct _stat64 fileStatus;
        if (_fstat64(file, &fileStatus) != 0)
        {
            // Unable to get size
            (void)_close(file);
            return -1;
        }

        // Check if file is valid
        if (!S_ISREG(fileStatus.st_mode))
        {
            // Not a regular file
            (void)_close(file);
            return -1;
        }

        // Check we aren't trying to read a size greater than the file
        const uint64_t fileLengthBytes = static_cast<uint64_t>(std::max<int64_t>(fileStatus.st_size, 0));
        if (fileLengthBytes < bufferLen)
            return -1;

        // If we requested reading from an offset within the file, move the read head now
        if (readOffset)
        {
            int64_t fileOffset = _lseeki64(file, readOffset, 0);
            if (fileOffset != readOffset)   // The offset from start of file should always match the requested read offset
                return -1;
        }

        // Fill in the buffer
        char* pFileBuffer = (char*)buffer;
        for (uint64_t bytesLeft = bufferLen; bytesLeft > 0;)
        {
            uint32_t bytesRequested = static_cast<uint32_t>(std::min<uint64_t>(bytesLeft, INT32_MAX));
            auto bytesReceivedOrStatus = _read(file, pFileBuffer, bytesRequested);

            // Break into 4GB chunks
            if (bytesReceivedOrStatus > 0)
            {
                bytesLeft -= bytesReceivedOrStatus;
                pFileBuffer += bytesReceivedOrStatus;
            }

            // Going past eof... not good
            else if (bytesReceivedOrStatus == 0)
            {
                (void)_close(file);
                return -1;
            }

            // Error reported during read
            else if (bytesReceivedOrStatus < 0)
            {
                (void)_close(file);
                return -1;
            }
        }

        // All done
        (void)_close(file);

        return bufferLen;
    }

    int64_t GetFileSize(const wchar_t* fileName)
    {
        // Try to open
        int file = -1;
        (void)_wsopen_s(&file, fileName, _O_RDONLY | _O_NOINHERIT | _O_BINARY | _O_SEQUENTIAL, _SH_DENYNO, _S_IREAD);

        // Can't open file for some reason
        if (file == -1)
            return -1;

        // Something went wrong. Permissions or not found
        struct _stat64 fileStatus;
        if (_fstat64(file, &fileStatus) != 0)
        {
            _close(file);
            return -1;
        }

        // Done
        (void)_close(file);

        return fileStatus.st_size;
    }

    bool ParseJsonFile(const wchar_t* fileName, json& jsonOut)
    {
        // Add any render modules needed for this sample
        int64_t fileSize = GetFileSize(fileName);
        if (fileSize == -1)
        {
            CauldronError(L"Could not get file size of %ls", fileName);
            return false;
        }

        char* pFileBuffer = new char[fileSize + 1];

        int64_t sizeRead = ReadFileAll(fileName, pFileBuffer, fileSize);
        pFileBuffer[sizeRead] = 0;
        if (sizeRead != fileSize)
        {
            delete[](pFileBuffer);  // Release allocated memory
            CauldronError(L"File %ls was not fully read due to error", fileName);
            return false;
        }

        // Fill in the json representation
        jsonOut = json::parse(pFileBuffer);

        // Can release the memory now
        delete[](pFileBuffer);

        return true;
    }

} // namespace cauldron
