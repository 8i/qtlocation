/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.0

TestCase {
    id: testCase

    name: "PositionSource"

    PositionSource { id: defaultSource }
    SignalSpy { id: defaultSourceSpy; target: defaultSource; signalName: "positionChanged" }

    function test_defaults() {
        // at least the test.source plugin should be available
        verify(defaultSource.name != "");
        compare(defaultSource.active, false);
        compare(defaultSource.updateInterval, 0);
    }

    function test_inactive() {
        compare(defaultSourceSpy.count, 0);
        wait(1000);
        compare(defaultSourceSpy.count, 0);
    }

    PositionSource { id: testSetSource; name: "nonexistent bogus plugin" }
    SignalSpy { id: testingSourcePluginSpy; target: testSetSource; signalName: "nameChanged" }

    function test_setplugin() {
        testingSourcePluginSpy.clear();

        testSetSource.name = "test.source";
        compare(testingSourcePluginSpy.count, 1);
        compare(testSetSource.name, "test.source");

        testSetSource.name = "test.source";
        compare(testingSourcePluginSpy.count, 1);

        testSetSource.name = "bogus";
        compare(testingSourcePluginSpy.count, 2);
    }

    PositionSource { id: testingSource; name: "test.source"; updateInterval: 1000 }
    SignalSpy { id: updateSpy; target: testingSource; signalName: "positionChanged" }

    function test_updateInterval() {
        testingSource.updateInterval = 1000;
        compare(testingSource.updateInterval, 1000);
        testingSource.updateInterval = 1200;
        compare(testingSource.updateInterval, 1200);
        testingSource.updateInterval = 800;
        compare(testingSource.updateInterval, 1000);
    }

    function test_updates() {
        updateSpy.clear();

        testingSource.active = true;

        tryCompare(updateSpy, "count", 1, 1500);
        compare(testingSource.position.coordinate.longitude, 0.1);
        compare(testingSource.position.coordinate.latitude, 0.1);

        tryCompare(updateSpy, "count", 2, 1500);
        compare(testingSource.position.coordinate.longitude, 0.2);
        compare(testingSource.position.coordinate.latitude, 0.2);

        testingSource.active = false;
        wait(2500);
        compare(updateSpy.count, 2);
        compare(testingSource.position.coordinate.longitude, 0.2);
        compare(testingSource.position.coordinate.latitude, 0.2);
    }
}
