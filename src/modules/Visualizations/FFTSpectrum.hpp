#include <QMPlay2Extensions.hpp>
#include <VisWidget.hpp>

#include <QLinearGradient>

class FFTSpectrum;

class FFTSpectrumW : public VisWidget
{
	friend class FFTSpectrum;
	Q_OBJECT
public:
	FFTSpectrumW(FFTSpectrum &);
private:
	void paintEvent(QPaintEvent *);

	void start(bool v = false, bool dontCheckRegion = false);
	void stop();

	QVector< float > spectrumData;
	QVector< QPair< qreal, QPair< qreal, double > > > lastData;
	uchar chn;
	uint srate;
	int interval, fftSize;
	FFTSpectrum &fftSpectrum;
	QLinearGradient linearGrad;
};

/**/

struct FFTContext;
struct FFTComplex;

class FFTSpectrum : public QMPlay2Extensions
{
public:
	FFTSpectrum(Module &);

	void soundBuffer(const bool);

	bool set();
private:
	DockWidget *getDockWidget();

	bool isVisualization() const;
	void connectDoubleClick(const QObject *, const char *);
	void visState(bool, uchar, uint);
	void sendSoundData(const QByteArray &);

	/**/

	FFTSpectrumW w;

	FFTContext *fft_ctx;
	FFTComplex *tmpData;
	int tmpDataSize, tmpDataPos, scale;
	QMutex mutex;
};

#define FFTSpectrumName "Widmo FFT"
