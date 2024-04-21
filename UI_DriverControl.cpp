#include "UI_DriverControl.h"

#include "WMFAACDecoder.h"

#include <initguid.h>
#include <Devpkey.h>
#include <Devpropdef.h>

#include <tchar.h>
#include <Cfgmgr32.h>
#include <winioctl.h>
#include <ks.h>

#include <QFile>

#include <array>
#include <cassert>
#include <iostream>

#include <iomanip>
#include <sstream>

#define RtlOffsetToPointer(Base, Offset) ((PCHAR)(((PCHAR)(Base)) + ((ULONG_PTR)(Offset))))
#define RtlPointerToOffset(Base, Pointer) ((ULONG)(((PCHAR)(Pointer)) - ((PCHAR)(Base))))

namespace
{
GUID AUDIO_DEVICE_CATEGORY_GUID = { 0x65E8773D, 0x8F56, 0x11D0, {0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96 } };

HANDLE getDrvHandle()
{
  DWORD err;
  constexpr std::size_t BufferLen = 1024 * 128;
  std::array<std::uint8_t, BufferLen> Buffer;

  ULONG NeedLen = BufferLen / 2;

  union
    {
    PVOID buf;
    PWCHAR pszDeviceInterface;
  };

  buf = Buffer.data();

  for (;;)
  {
    if (BufferLen < NeedLen)
    {
      assert(false);
      return INVALID_HANDLE_VALUE;
      //BufferLen = RtlPointerToOffset(buf = alloca((NeedLen - BufferLen) * sizeof(WCHAR)), stack) / sizeof(WCHAR);
    }

    switch (err = CM_Get_Device_Interface_ListW(const_cast<LPGUID>(&AUDIO_DEVICE_CATEGORY_GUID),
                                                0, pszDeviceInterface, BufferLen, CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES))
    {
      case CR_BUFFER_SMALL:
        if (err = CM_Get_Device_Interface_List_SizeW(&NeedLen, const_cast<LPGUID>(&AUDIO_DEVICE_CATEGORY_GUID),
                                                     0, CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES))
        {
          default:
            return INVALID_HANDLE_VALUE;
        }
        continue;

      case CR_SUCCESS:

        while (*pszDeviceInterface)
        {
          ULONG keysCount = 256;
          DEVPROPKEY keys[256];
          CM_Get_Device_Interface_Property_KeysW(pszDeviceInterface, keys, &keysCount, 0);

#if PRINT_DEVICES
          wprintf(pszDeviceInterface);
          printf("\n");
#endif

          for (int i = 0; i < keysCount; ++i)
          {
            // std::cout << (keys[0].pid) << std::endl << std::flush;
#if PRINT_DEVICES
            const auto& guid = keys[i].fmtid;
            printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX, %d}\n",
                   guid.Data1, guid.Data2, guid.Data3,
                   guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                   guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7], keys[i].pid);
#endif

            // if (IsEqualDevPropKey(keys[i], DEVPKEY_DeviceInterface_FriendlyName))
            {
              DEVPROPTYPE type;
              ULONG bufLen = 2048;
              BYTE buf[2048];
              CM_Get_Device_Interface_PropertyW(pszDeviceInterface, &keys[i], &type, buf, &bufLen, 0);

              if (type == DEVPROP_TYPE_STRING)
              {
#if PRINT_DEVICES
                printf(" - ");
                wprintf((PWSTR)buf);
                printf("\n");
#endif

                constexpr auto MicBridgeInterfaceName = L"micBridgeWaveCapture";

                if (wcscmp((PWSTR)buf, MicBridgeInterfaceName) == 0)
                {
                  HANDLE hFile = CreateFileW(pszDeviceInterface,
                                                           GENERIC_READ | GENERIC_WRITE,
                                                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                           0,
                                             OPEN_EXISTING, 0, 0);

                  if (err == CR_SUCCESS)
                  {
                    HANDLE hFile = CreateFileW(pszDeviceInterface,
                                               GENERIC_READ | GENERIC_WRITE,
                                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               0,
                                               OPEN_EXISTING, 0, 0);

                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                      _tprintf(TEXT("NOT  -   OK"));
                      _tprintf(TEXT("\n"));
                    }
                  }

                  return hFile;
                }
              }
            }
          }

          pszDeviceInterface += 1 + lstrlenW(pszDeviceInterface);
        }
        return INVALID_HANDLE_VALUE;
    }
  }
}

AudioInfo::AudioFormat getDefaultAudioFormat()
{
    return {44100, 1, 16, true, true};
}
} // namespace

wmf::WMFAACDecoder decoder;

DriverControlFramesSender::DriverControlFramesSender()
    : driverHandle_(getDrvHandle())
    , audioInfo_(getDefaultAudioFormat(), nullptr)
{
    HMODULE mod = LoadLibrary(wmf::WMFDecoderDllNameFor(wmf::CodecType::AAC));
    int err = GetLastError();
    LoadLibrary("mfplat.dll");

    unsigned char audioSpecificConfig[2];
    std::uint8_t const audioObjectType = 2;
    audioSpecificConfig[0] = (audioObjectType << 3) | (4 >> 1);
    audioSpecificConfig[1] = (4 << 7) | (1 << 3);

    decoder.Init(1, 44100, audioSpecificConfig, 2);
}

AudioInfo* DriverControlFramesSender::getAudioInfoIODevice()
{
    return &audioInfo_;
}

void DriverControlFramesSender::onFrame(const std::uint8_t *frameData, std::size_t length)
{
//    static QFile* f = []()
//    {
//        QFile* file = new QFile("out.aac");
//        file->open(QIODevice::WriteOnly | QIODevice::Append);

//        return file;
//    }();

//    f->write(reinterpret_cast<const char*>(frameData), length);

    onDecodedData(frameData, length);
//    if (SUCCEEDED(decoder.Input(frameData, length, 0)))
//    {
//        wmf::CComPtr<IMFSample> input = nullptr;
//        if (SUCCEEDED(decoder.Output(&input)))
//        {
//            wmf::CComPtr<IMFMediaBuffer> buffer = nullptr;
//            HRESULT hr = input->ConvertToContiguousBuffer(&buffer);
//            if (!SUCCEEDED(hr))
//                std::cout << "Couldn't convert " << hr << std::endl << std::flush;
//            DWORD maxLength = 0;
//            DWORD currentLength = 0;
//            BYTE* bufferBytes = nullptr;
//            hr = buffer->Lock(&bufferBytes, &maxLength, &currentLength);
//            if (!SUCCEEDED(hr))
//            {
//                std::cout << "Lock buffer not succeeded: " << hr << std::endl << std::flush;
//            }
//            else
//            {
//                onDecodedData(bufferBytes, currentLength);
//            }

//            buffer->Unlock();
//        }
//        else
//        {
//            // std::cout << "Decoder output failed \n" << std::endl << std::flush;
//        }
//    }
//    else
//    {
//        std::cout << "Decoder input failed \n" << std::endl << std::flush;
//    }
}

namespace
{

std::string getTimestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto nowAsTimeT = system_clock::to_time_t(now);
    const auto nowMs = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream nowSs;
    nowSs << std::put_time(std::localtime(&nowAsTimeT), "%a %b %d %Y %T")
          << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
    return nowSs.str();
}

}  // namespace

void DriverControlFramesSender::onDecodedData(const std::uint8_t* frameData, std::size_t length)
{
//    static std::size_t rawPacketsCount = 0;
//    static std::size_t rawTotalBytes = 0;

//    rawPacketsCount++;
//    rawTotalBytes += length;

//    if ((rawPacketsCount % 100) == 0)
//    {
//        std::cout << "Raw packets [" << rawPacketsCount << " pckts," << rawTotalBytes << " bytes] - "
//                  << "decoded. Timestamp: " << getTimestamp() << std::endl;
//    }

    if (driverHandle_ != INVALID_HANDLE_VALUE)
    {
        KSSTREAM_HEADER streamHeader;
        ZeroMemory(&streamHeader, sizeof(KSSTREAM_HEADER));
        streamHeader.Size = sizeof(KSSTREAM_HEADER);
        streamHeader.Data = const_cast<std::uint8_t*>(frameData);
        streamHeader.FrameExtent = length;
        streamHeader.DataUsed = 0;
        streamHeader.PresentationTime.Time = 0;
        streamHeader.PresentationTime.Numerator = 1;
        streamHeader.PresentationTime.Denominator = 1;
        streamHeader.OptionsFlags = false ? KSSTREAM_HEADER_OPTIONSF_LOOPEDDATA : 0;

        DWORD cbReturned = 0;
        DeviceIoControl(driverHandle_,
                        IOCTL_KS_READ_STREAM,
                        &streamHeader,
                        sizeof(KSSTREAM_HEADER),
                        NULL,
                        NULL,
                        &cbReturned,
                        NULL);
    }

    audioInfo_.writeData(reinterpret_cast<const char*>(frameData), length);
}
