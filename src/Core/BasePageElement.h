/*******************************************************************************************************
 ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ. 
  
 Copyright (C) 2016 Markus Diem <diem@cvl.tuwien.ac.at>
 Copyright (C) 2016 Stefan Fiel <fiel@cvl.tuwien.ac.at>
 Copyright (C) 2016 Florian Kleber <kleber@cvl.tuwien.ac.at>

 This file is part of ReadFramework.

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

#pragma once

#pragma warning(push, 0)	// no warnings from includes
#include <QObject>
#pragma warning(pop)

#ifndef DllExport
#ifdef DLL_CORE_EXPORT
#define DllExport Q_DECL_EXPORT
#else
#define DllExport Q_DECL_IMPORT
#endif
#endif

// Qt defines

namespace pie {

// read defines

class DllExport BaseElement {

public:
	BaseElement(const QString& id = QString());

	DllExport friend bool operator==(const BaseElement& l, const QString& id);
	DllExport friend bool operator==(const BaseElement& l, const BaseElement& r);
	DllExport friend bool operator!=(const BaseElement& l, const BaseElement& r);
	DllExport friend QDataStream& operator<<(QDataStream& s, const BaseElement& e);
	DllExport friend QDebug operator<< (QDebug d, const BaseElement &e);

	void setId(const QString& id);
	QString id() const;
	virtual QString toString() const;

	virtual void scale(double factor);

protected:
	QString mId;
};

}
