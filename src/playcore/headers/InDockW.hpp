#ifndef INDOCKW_HPP
#define INDOCKW_HPP

#include <QWidget>

#include "export.h"

class LIBEXPORT InDockW : public QWidget
{
	Q_OBJECT
public:
	InDockW(const QPixmap &, const QColor &, const QColor &, const QColor &);

	inline QWidget *widget()
	{
		return w;
	}
	inline void setLoseHeight(int lh)
	{
		loseHeight = lh;
	}
	void setCustomPixmap(const QPixmap &pix = QPixmap());
private:
	const QColor &grad1, &grad2, &qmpTxt;
	const QPixmap &qmp2Pixmap;
	QPixmap customPixmap;
	bool hasWallpaper;
	int loseHeight;
	QWidget *w;
private slots:
	void wallpaperChanged(bool hasWallpaper, double alpha);
	void setWidget(QWidget *);
	void nullWidget();
protected:
	void resizeEvent(QResizeEvent *);
	void paintEvent(QPaintEvent *);
	bool event(QEvent *);
signals:
	void resized(int, int);
	void itemDropped(const QString &);
};

#endif //INDOCKW_HPP
