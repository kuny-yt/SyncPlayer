#include <QPainterWriter.hpp>

#include <QMPlay2_OSD.hpp>
#include <VideoFrame.hpp>
#include <Functions.hpp>
using Functions::getImageSize;
using Functions::aligned;

#include <QCoreApplication>
#include <QPainter>

/**/

Drawable::Drawable(QPainterWriter &writer) :
	writer(writer)
{
	grabGesture(Qt::PinchGesture);
	setAutoFillBackground(true);
	setMouseTracking(true);
	setPalette(Qt::black);
}
Drawable::~Drawable()
{
	clr();
}

void Drawable::draw(const VideoFrame &newVideoFrame, bool canRepaint, bool entireScreen)
{
	if (!newVideoFrame.isEmpty())
		videoFrame = newVideoFrame;
	else if (videoFrame.isEmpty())
	{
		update();
		return;
	}
	if (imgScaler.create(writer.outW, writer.outH, W, H))
	{
		if (img.isNull())
			img = QImage(W, H, QImage::Format_RGB32);
		imgScaler.scale(videoFrame, img.bits());
		if (writer.flip)
			img = img.mirrored(writer.flip & Qt::Horizontal, writer.flip & Qt::Vertical);
		if (Brightness != 0 || Contrast != 100)
			Functions::ImageEQ(Contrast, Brightness, img.bits(), W * H << 2);
	}
	if (canRepaint && !entireScreen)
		update(X, Y, W, H);
	else if (canRepaint && entireScreen)
		update();
}
void Drawable::clr()
{
	imgScaler.destroy();
	img = QImage();
}

void Drawable::resizeEvent(QResizeEvent *e)
{
	getImageSize(writer.aspect_ratio, writer.zoom, width(), height(), W, H, &X, &Y);
	W = aligned(W, 8);

	clr();
	draw(VideoFrame(), e ? false : true, true);
}
void Drawable::paintEvent(QPaintEvent *)
{
	QPainter p(this);

	p.translate(X, Y);
	p.drawImage(0, 0, img);

	osd_mutex.lock();
	if (!osd_list.isEmpty())
	{
		p.setClipRect(0, 0, W, H);
		Functions::paintOSD(true, osd_list, (qreal)W / writer.outW, (qreal)H / writer.outH, p);
	}
	osd_mutex.unlock();
}
bool Drawable::event(QEvent *e)
{
	/* Pass gesture event to the parent */
	if (e->type() == QEvent::Gesture)
		return qApp->notify(parent(), e);
	return QWidget::event(e);
}

/**/

QPainterWriter::QPainterWriter(Module &module) :
	outW(-1), outH(-1), flip(0),
	aspect_ratio(0.0), zoom(0.0),
	drawable(NULL)
{
	addParam("W");
	addParam("H");
	addParam("AspectRatio");
	addParam("Zoom");
	addParam("Flip");
	addParam("Brightness");
	addParam("Contrast");

	SetModule(module);
}
QPainterWriter::~QPainterWriter()
{
	delete drawable;
}

bool QPainterWriter::set()
{
	return sets().getBool("Enabled");
}

bool QPainterWriter::readyWrite() const
{
	return drawable;
}

bool QPainterWriter::processParams(bool *)
{
	if (!drawable)
		drawable = new Drawable(*this);

	bool doResizeEvent = false;

	const double _aspect_ratio = getParam("AspectRatio").toDouble();
	const double _zoom = getParam("Zoom").toDouble();
	const int _flip = getParam("Flip").toInt();
	const int Contrast = getParam("Contrast").toInt() + 100;
	const int Brightness = getParam("Brightness").toInt() * 256 / 100;
	if (_aspect_ratio != aspect_ratio || _zoom != zoom || _flip != flip || Contrast != drawable->Contrast || Brightness != drawable->Brightness)
	{
		zoom = _zoom;
		aspect_ratio = _aspect_ratio;
		flip = _flip;
		drawable->Contrast = Contrast;
		drawable->Brightness = Brightness;
		doResizeEvent = drawable->isVisible();
	}

	const int _outW = getParam("W").toInt();
	const int _outH = getParam("H").toInt();
	if (_outW > 0 && _outH > 0 && (_outW != outW || _outH != outH))
	{
		drawable->videoFrame.clear();

		outW = _outW;
		outH = _outH;

		emit QMPlay2Core.dockVideo(drawable);
	}

	if (doResizeEvent)
		drawable->resizeEvent(NULL);

	return readyWrite();
}

void QPainterWriter::writeVideo(const VideoFrame &videoFrame)
{
	drawable->draw(videoFrame, true, false);
}
void QPainterWriter::writeOSD(const QList< const QMPlay2_OSD * > &osds)
{
	drawable->osd_mutex.lock();
	drawable->osd_list = osds;
	drawable->osd_mutex.unlock();
}

QString QPainterWriter::name() const
{
	return QPainterWriterName;
}

bool QPainterWriter::open()
{
	return true;
}
