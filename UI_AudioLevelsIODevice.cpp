#include "UI_AudioLevelsIODevice.h"

#include <QtEndian>

AudioInfo::AudioInfo(const AudioFormat &format, QObject *parent)
    : QIODevice(parent)
    , m_format(format)
    , m_maxAmplitude(0)
    , m_level(0.0)

{
  switch (m_format.sampleLength) {
    case 8:
      switch (m_format.isSigned) {
        case false:
          m_maxAmplitude = 255;
          break;
        case true:
          m_maxAmplitude = 127;
          break;
        default: ;
      }
      break;
    case 16:
      switch (m_format.isSigned) {
        case false:
          m_maxAmplitude = 65535;
          break;
        case true:
          m_maxAmplitude = 32767;
          break;
        default: ;
      }
      break;
  }
}

AudioInfo::~AudioInfo()
{
}

void AudioInfo::start()
{
  open(QIODevice::WriteOnly);
}

void AudioInfo::stop()
{
  close();
}

qint64 AudioInfo::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data);
    Q_UNUSED(maxlen);
    return 0;
//    return buffer_.readBuff(data, maxlen);
}

qint64 AudioInfo::writeData(const char *data, qint64 len)
{
//    const auto written = buffer_.writeBuff(data, len);
//    emit bytesWritten(written);

  if (m_maxAmplitude) {
    Q_ASSERT(m_format.sampleLength % 8 == 0);
    const int channelBytes = m_format.sampleLength / 8;
    const int sampleBytes = m_format.channelsCount * channelBytes;
    Q_ASSERT(len % sampleBytes == 0);
    const int numSamples = len / sampleBytes;

    quint16 maxValue = 0;
    const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);

    for (int i = 0; i < numSamples; ++i)
    {
      for(int j = 0; j < m_format.channelsCount; ++j)
        {
        quint16 value = 0;

        if (m_format.sampleLength == 8 && !m_format.isSigned) {
          value = *reinterpret_cast<const quint8*>(ptr);
        } else if (m_format.sampleLength == 8 && m_format.isSigned) {
          value = qAbs(*reinterpret_cast<const qint8*>(ptr));
        } else if (m_format.sampleLength == 16 && !m_format.isSigned) {
          if (m_format.isLittleEndian)
            value = qFromLittleEndian<quint16>(ptr);
          else
            value = qFromBigEndian<quint16>(ptr);
        } else if (m_format.sampleLength == 16 && m_format.isSigned) {
          if (m_format.isLittleEndian)
            value = qAbs(qFromLittleEndian<qint16>(ptr));
          else
            value = qAbs(qFromBigEndian<qint16>(ptr));
        }

        maxValue = qMax(value, maxValue);
        ptr += channelBytes;
      }
    }

    maxValue = qMin(maxValue, m_maxAmplitude);
    m_level = qreal(maxValue) / m_maxAmplitude;
  }

  emit update(m_level);

  return len;
}
