/*******************************************************************************************************
 PIE is the Page Image Explorer developed at CVL/TU Wien for the EU project READ.

 Copyright (C) 2018 Markus Diem <diem@caa.tuwien.ac.at>
 Copyright (C) 2018 Florian Kleber <kleber@caa.tuwien.ac.at>

 This file is part of PIE.

 ReadFramework is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 ReadFramework is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 The READ project  has  received  funding  from  the European  Union’s  Horizon  2020
 research  and innovation programme under grant agreement No 674943

 related links:
 [1] https://cvl.tuwien.ac.at/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] https://nomacs.org
 *******************************************************************************************************/


#include "Utils.h"
#include "Network.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QApplication>
#include <QDebug>
#include <QPolygon>
#include <QFileInfo>
#include <QDir>
#include <QTime>
#include <QColor>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUrl>
#include <QDateTime>
#include <QPixmap>
#include <QPainter>

#include <opencv2/core.hpp>
#pragma warning(pop)

// needed for registering the file version
#ifdef WIN32
#include "shlwapi.h"
#pragma comment (lib, "shlwapi.lib")
#endif


namespace pie {


// Utils --------------------------------------------------------------------
Utils::Utils() {

	// initialize random
	qsrand(QTime::currentTime().msec());
}

Utils& Utils::instance() { 

	static QSharedPointer<Utils> inst;
	if (!inst)
		inst = QSharedPointer<Utils>(new Utils());
	return *inst; 
}

bool Utils::hasGui() {
	return (qobject_cast<QApplication*>(QCoreApplication::instance()) != 0);	// check if only QCoreApplication (headless) is running
}

void Utils::initFramework() const {

	// format console
	QString p = "%{if-info}[INFO] %{endif}%{if-warning}[WARNING] %{endif}%{if-critical}[CRITICAL] %{endif}%{if-fatal}[ERROR] %{endif}%{message}";
	qSetMessagePattern(p);

	registerVersion();
}

void Utils::registerVersion() const {

#ifdef WIN32
	// this function is based on code from:
	// http://stackoverflow.com/questions/316626/how-do-i-read-from-a-version-resource-in-visual-c

	QString version(PIE_FRAMEWORK_VERSION);	// default version (we do not know the build)
	
	// get the filename of the executable containing the version resource
	TCHAR szFilename[MAX_PATH + 1] = {0};
	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0) {
		qWarning() << "Sorry, I can't read the module fileInfo name";
		return;
	}

	// allocate a block of memory for the version info
	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0) {
		qWarning() << "The version info size is zero\n";
		return;
	}
	QVector<BYTE> bytes(dwSize);

	if (bytes.empty()) {
		qWarning() << "The version info is empty\n";
		return;
	}

	// load the version info
	if (!bytes.empty() && !GetFileVersionInfo(szFilename, NULL, dwSize, &bytes[0])) {
		qWarning() << "Sorry, I can't read the version info\n";
		return;
	}

	// get the name and version strings
	UINT                uiVerLen = 0;
	VS_FIXEDFILEINFO*   pFixedInfo = 0;     // pointer to fixed file info structure

	if (!bytes.empty() && !VerQueryValue(&bytes[0], TEXT("\\"), (void**)&pFixedInfo, (UINT *)&uiVerLen)) {
		qWarning() << "Sorry, I can't get the version values...\n";
		return;
	}

	// pFixedInfo contains a lot more information...
	version = QString::number(HIWORD(pFixedInfo->dwFileVersionMS)) + "."
		+ QString::number(LOWORD(pFixedInfo->dwFileVersionMS)) + "."
		+ QString::number(HIWORD(pFixedInfo->dwFileVersionLS)) + "."
		+ QString::number(LOWORD(pFixedInfo->dwFileVersionLS));

#else
	QString version(PIE_FRAMEWORK_VERSION);	// default version (we do not know the build)
#endif
	QApplication::setApplicationVersion(version);

}

/// <summary>
/// Combines a version number into one int.
/// Version numbers thus converted can be queried (v1 < v2).
/// Version numbers are assumed to be VERSION.MAJOR.MINOR (e.g. 3.4.1).
/// </summary>
/// <param name="major">The major version.</param>
/// <param name="minor">The minor version.</param>
/// <param name="revision">The revision.</param>
/// <returns></returns>
int Utils::versionToInt(char major, char minor, char revision) {
	
	return major << 16 | minor << 8 | revision;
}

/// <summary>
/// Returns a random number within [0 1].
/// </summary>
/// <returns></returns>
double Utils::rand() {

	return (double)qrand() / RAND_MAX;
}

/// <summary>
/// Loads file to buffer.
/// The file is either loaded from a local resource
/// or from a network resource (blocking!).
/// </summary>
/// <param name="filePath">The file path or url.</param>
/// <param name="ba">The buffer.</param>
/// <returns>true if the resource was loaded.</returns>
bool Utils::loadToBuffer(const QString & filePath, QByteArray & ba) {
	
	if (QFileInfo(filePath).exists()) {

		QFile f(filePath);

		if (!f.open(QIODevice::ReadOnly)) {
			qWarning() << "Sorry, I could not open " << filePath << " for reading...";
			return false;
		}

		// load the element
		ba = f.readAll();
		f.close();
	}
	// if there is no local resource - try downloading it
	else if (QUrl(filePath).isValid()) {

		bool ok = false;
		ba = net::download(filePath, &ok);

		if (!ok)
			return false;
	}
	else {
		qCritical() << "cannot read from non-existing file:" << filePath;
		return false;
	}

	// all good here...
	return true;
}

/// <summary>
/// Returns the path for writing persistant application data.
/// The path refers to GenericDataLocation/organizationName.
/// On Windows e.g. C:\Users\markus\AppData\Local\TU Wien
/// </summary>
/// <returns>The app path.</returns>
QString Utils::appDataPath() {

	QString adp;

#if QT_VERSION >= 0x050000
	adp = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#else
	adp = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif

	// make our own folder
	adp += QDir::separator() + QCoreApplication::organizationName();

	if (!QDir().mkpath(adp))
		qWarning() << "I could not create" << adp;

	return adp;
}

/// <summary>
/// Creates a new file path from filePath.
/// Hence, C:\temp\josef.png can be turned into C:\temp\josef-something.xml
/// </summary>
/// <param name="filePath">The old file path.</param>
/// <param name="attribute">An attribute string which is appended to the filename.</param>
/// <param name="newSuffix">A new suffix, the old suffix is used if empty.</param>
/// <returns>The new file path.</returns>
QString Utils::createFilePath(const QString & filePath, const QString & attribute, const QString & newSuffix) {
	
	QFileInfo info(filePath);
	QString suffix = (newSuffix.isEmpty()) ? info.suffix() : newSuffix;
	QString newFilePath = baseName(filePath) + attribute + "." + suffix;

	return newFilePath;
}

/// <summary>
/// Returns a 'unique' filename named "ATTRIBUTE YYYY-MM-dd HH-mm.SUFFIX".
/// </summary>
/// <param name="attribute">An optional string to specify the file.</param>
/// <param name="suffix">The file suffix.</param>
/// <returns></returns>
QString Utils::timeStampFileName(const QString & attribute, const QString & suffix) {
	
	QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
	
	return attribute + " " + ts + suffix;
}

/// <summary>
/// Returns the filePath without suffix.
/// C:/temp/something.png -> C:/temp/something
/// This fixes an issue of Qt QFileInfo::baseName which 
/// returns wrong basenames if the filename contains dots
/// Qt baseName:
/// Best. 901 Nr. 112 00147.jpg -> Best.
/// This method:
/// Best. 901 Nr. 112 00147.jpg -> Best. 901 Nr. 112 00147
/// </summary>
/// <param name="filePath">The file path.</param>
/// <returns>The file path without suffix.</returns>
QString Utils::baseName(const QString & filePath) {

	QString suffix = QFileInfo(filePath).suffix();

	int sI = filePath.lastIndexOf(suffix);

	if (sI < 1) {
		qWarning() << "Cannot extract basename:" << filePath << "does not have a suffix";
		return filePath;
	}

	return filePath.left(sI-1);	// -1 to remove the point
}

/// <summary>
/// Converts the color to a qss legitimate string.
/// #ff0000 would be converted to "rgba(255,0,0,100)".
/// </summary>
/// <param name="col">The color.</param>
/// <returns>The color's qss string</returns>
QString Utils::colorToString(const QColor & col) {

	return "rgba(" + QString::number(col.red()) +
		"," + QString::number(col.green()) +
		"," + QString::number(col.blue()) +
		"," + QString::number((float)col.alpha() / 255.0f*100.0f) + "%)";
}

QJsonObject Utils::readJson(const QString & filePath) {

	if (filePath.isEmpty()) {
		qCritical() << "cannot read Json, file path is empty...";
		return QJsonObject();
	}

	QByteArray ba;
	if (!Utils::loadToBuffer(filePath, ba)) {
		qCritical() << "cannot read Json from" << filePath;
		return QJsonObject();
	}

	QJsonDocument doc = QJsonDocument::fromJson(ba);
	if (doc.isNull() || doc.isEmpty()) {
		qCritical() << "cannot parse NULL document: " << filePath;
		return QJsonObject();
	}

	return doc.object();
}

int64 Utils::writeJson(const QString & filePath, const QJsonObject & jo) {

	if (filePath.isEmpty()) {
		qCritical() << "cannot write Json, file path is empty...";
		return 0;
	}

	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QFileInfo fi(filePath);

		if (!fi.exists())
			qCritical() << "cannot open or write to" << filePath;

		return 0;
	}

	QJsonDocument doc(jo);
	int64 nb = file.write(doc.toJson());

	if (nb == -1)
		qCritical() << "could not write data to" << filePath;
	else
		qDebug() << nb << "bytes written to" << filePath;

	return 0;
}

void Utils::initDefaultFramework() {

	QCoreApplication::setOrganizationName("TU Wien");
	QCoreApplication::setOrganizationDomain("http://www.caa.tuwien.ac.at/cvl");
	QCoreApplication::setApplicationName("READ Framework");

	Utils::instance().initFramework();

}

bool Utils::isValidFile(const QString & filePath) {

	QFileInfo fInfo(filePath);
	if (fInfo.isSymLink())
		fInfo = fInfo.symLinkTarget();

	if (!fInfo.exists())
		return false;

	QString fileName = fInfo.fileName();

	QStringList fileFilters;
	fileFilters << "*.json";

	for (const QString& f : fileFilters) {

		QRegExp exp = QRegExp(f, Qt::CaseInsensitive);
		exp.setPatternSyntax(QRegExp::Wildcard);
		if (exp.exactMatch(fileName))
			return true;
	}

	qDebug() << filePath << "is not valid...";

	return false;
}

// Converter --------------------------------------------------------------------

/// <summary>
/// Converts a cv::Rect to QRectF.
/// </summary>
/// <param name="r">The OpenCV rectangle.</param>
/// <returns></returns>
QRectF Converter::cvRectToQt(const cv::Rect & r) {
	return QRectF(r.x, r.y, r.width, r.height);
}

/// <summary>
/// Converts a QRectF to a cv::Rect.
/// </summary>
/// <param name="r">The Qt rectangle.</param>
/// <returns></returns>
cv::Rect2d Converter::qRectToCv(const QRectF & r) {
	return cv::Rect2d(r.x(), r.y(), r.width(), r.height());
}

/// <summary>
/// Converts a PAGE points attribute to a polygon.
/// the format is: p1x,p1y p2x,p2y (for two points p1, p2)
/// </summary>
/// <param name="pointList">A string containing the point list.</param>
/// <returns>A QPolygon parsed from the point list.</returns>
QPolygon Converter::stringToPoly(const QString& pointList) {

	// we expect point pairs like that: <Coords points="1077,482 1167,482 1167,547 1077,547"/>
	QStringList pairs = pointList.split(" ");
	QPolygon poly;

	for (const QString pair : pairs) {

		QStringList points = pair.split(",");

		if (points.size() != 2) {
			qWarning() << "illegal point string: " << pair;
			continue;
		}

		bool xok = false, yok = false;
		int x = points[0].toInt(&xok);
		int y = points[1].toInt(&yok);

		if (xok && yok)
			poly.append(QPoint(x, y));
		else
			qWarning() << "illegal point string: " << pair;
	}

	return poly;
}

/// <summary>
/// Converts a QPolygon to QString according to the PAGE XML format.
/// A line would look like this: p1x,p1y p2x,p2y
/// </summary>
/// <param name="polygon">The polygon to convert.</param>
/// <returns>A string representing the polygon.</returns>
QString Converter::polyToString(const QPolygon& polygon) {

	QString polyStr;

	for (const QPoint& p : polygon) {
		polyStr += QString::number(p.x()) + "," + QString::number(p.y()) + " ";
	}

	// NOTE: we have one space at the end
	//FK040716: remove last space - otherwise we get a warning when reading
	polyStr.remove(polyStr.length() - 1, 1);

	return polyStr;
}

QPointF Converter::cvPointToQt(const cv::Point & pt) {
	return QPointF(pt.x, pt.y);
}

cv::Point2d Converter::qPointToCv(const QPointF & pt) {
	return cv::Point2d(pt.x(), pt.y());
}

// Timer --------------------------------------------------------------------
/**
* Initializes the class and stops the clock.
**/
Timer::Timer() {
	mTimer.start();
}

QDataStream& operator<<(QDataStream& s, const Timer& t) {

	// this makes the operator<< virtual (stroustrup)
	return t.put(s);
}

QDebug operator<<(QDebug d, const Timer& t) {

	d << qPrintable(t.stringifyTime(t.elapsed()));
	return d;
}

/**
* Returns a string with the total time interval.
* The time interval is measured from the time,
* the object was initialized.
* @return the time in seconds or milliseconds.
**/
QString Timer::getTotal() const {

	return qPrintable(stringifyTime(mTimer.elapsed()));
}

QDataStream& Timer::put(QDataStream& s) const {

	s << stringifyTime(mTimer.elapsed());

	return s;
}

/**
* Converts time to QString.
* @param ct current time interval
* @return QString the time interval as string
**/ 
QString Timer::stringifyTime(int ct) const {

	if (ct < 1000)
		return QString::number(ct) + " ms";

	int v = qRound(ct / 1000.0);
	int ms = ct % 1000;
	int sec = v % 60;	v = qRound(v / 60.0);
	int min = v % 60;	v = qRound(v / 60.0);
	int h = v % 24;		v = qRound(v / 24.0);
	int d = v;
	
	QString mss = QString("%1").arg(ms, 3, 10, QChar('0')); // zero padding
	QString secs = QString::number(sec);

	if (ct < 10000)
		return secs + "." + mss + " sec";

	if (ct < 60000)
		return secs + " sec";

	QString ds = QString("%1").arg(d, 2, 10, QChar('0'));		// zero padding e.g. 01
	QString hs = QString("%1").arg(h, 2, 10, QChar('0'));		// zero padding e.g. 01;
	QString mins = QString("%1").arg(min, 2, 10, QChar('0'));	// zero padding e.g. 01;
	secs = QString("%1").arg(sec, 2, 10, QChar('0'));

	if (ct < 3600000)
		return mins + ":" + secs;
	if (d == 0)
		return hs + ":" + mins + ":" + secs;

	return ds + "days " + hs + ":" + mins + ":" + secs;
}

void Timer::start() {
	mTimer.restart();
}

int Timer::elapsed() const {
	return mTimer.elapsed();
}

// ColorManager --------------------------------------------------------------------
/// <summary>
/// Returns a pleasent color.
/// </summary>
/// <param name="idx">If idx != -1 a specific color is chosen from the palette.</param>
/// <returns></returns>
QColor ColorManager::randColor(double alpha) {

	QVector<QColor> cols = colors();
	int maxCols = cols.size();

	int	idx = qRound(Utils::rand()*maxCols * 3);

	return color(idx, alpha);
}

/// <summary>
/// Returns a pleasent color.
/// </summary>
/// <param name="idx">If idx != -1 a specific color is chosen from the palette.</param>
/// <returns></returns>
QColor ColorManager::color(int idx, double alpha) {

	assert(idx >= 0);

	QVector<QColor> cols = colors();
	assert(cols.size() > 0);

	QColor col = cols[idx % cols.size()];

	// currently not hit
	if (idx > 2 * cols.size())
		col = col.lighter();
	else if (idx > cols.size())
		col = col.darker();

	col.setAlpha(qRound(alpha * 255));

	return col;
}

/// <summary>
/// Returns our color palette.
/// </summary>
/// <returns></returns>
QVector<QColor> ColorManager::colors() {

	static QVector<QColor> cols;

	if (cols.empty()) {
		cols << QColor(115, 0, 93);
		cols << QColor(230, 23, 190);
		cols << QColor(102, 80, 10);
		cols << QColor(230, 178, 11);
		cols << QColor(15, 153, 138);
		cols << QColor(102, 180, 10);
		cols << QColor(15, 253, 138);
	}

	return cols;
}

/// <summary>
/// Returns a light gray.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::lightGray(double alpha) {
	return QColor(200, 200, 200, qRound(alpha * 255));
}

/// <summary>
/// Returns a dark gray.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::darkGray(double alpha) {
	return QColor(66, 66, 66, qRound(alpha * 255));
}

/// <summary>
/// Returns a dark red.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::red(double alpha) {
	return QColor(200, 50, 50, qRound(alpha * 255));
}

/// <summary>
/// Returns a light green.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
DllExport QColor ColorManager::green(double alpha) {

	return QColor(120, 192, 167, qRound(alpha * 255));
}

/// <summary>
/// Returns the TU Wien blue.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::blue(double alpha) {
	return QColor(0, 102, 153, qRound(alpha * 255));
}

/// <summary>
/// Returns a pink color - not the artist.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::pink(double alpha) {
	return QColor(255, 0, 127, qRound(alpha * 255));
}

/// <summary>
/// Returns white - yes it's #fff.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::white(double alpha) {
	return QColor(255, 255, 255, qRound(alpha * 255));
}

/// <summary>
/// Returns black - no, it's not NULL.
/// </summary>
/// <param name="alpha">Optional alpha [0 1].</param>
/// <returns></returns>
QColor ColorManager::black(double alpha) {
	return QColor(0, 0, 0, qRound(alpha * 255));
}

QColor ColorManager::alpha(const QColor & col, double a) {

	QColor c = col;
	c.setAlphaF(a);

	return c;
}

/// <summary>
/// Colorizes the pixmap.
/// </summary>
/// <param name="icon">The (gray) pixmap.</param>
/// <param name="col">The destination color.</param>
/// <param name="opacity">The opacity, if != 1 the color is alpha blended with the original color.</param>
/// <returns>The colorized pixmap</returns>
QPixmap ColorManager::colorizePixmap(const QPixmap& pm, const QColor& col, double opacity) {

	if (pm.isNull())
		return pm;

	QPixmap pmc = pm.copy();
	QPixmap cpm = pm.copy();
	cpm.fill(col);

	QPainter p(&pmc);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.setOpacity(opacity);
	p.drawPixmap(cpm.rect(), cpm);

	return pmc;
}
// -------------------------------------------------------------------- ThemeManager 
ThemeManager & ThemeManager::instance() {
	
	static ThemeManager i;
	return i;
}

QColor ThemeManager::background() const {
	return QColor(255, 255, 255);
}

QColor ThemeManager::foreground() const {
	return QColor(0, 0, 0);
}

ThemeManager::ThemeManager() {
}

}