#pragma once

#include <QWheelEvent>

// Called only by the opt-in placement fixture with an isolated data folder.
// Send application-level wheel events through the real MainWindow input path.
inline void scheduleLayerScrollProbe(QApplication& app, const QString& folder, Position target)
{
    auto report = [folder](const QString& text) {
        QFile file(folder + "/result.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            file.write((text + "\n").toUtf8());
    };
    auto capture = [folder](const QString& name) {
        qputenv("INGNOMIA_UI_CAPTURE",(folder+"/"+name+".png").toUtf8());
        qputenv("INGNOMIA_UI_CAPTURE_FRAME","1");
        MainWindow::getInstance().armUiCapture();
    };
    auto wheel = [report](int delta, Qt::KeyboardModifiers modifiers, bool zoom) {
        auto& window = MainWindow::getInstance();
        auto* renderer = window.renderer();
        const int beforeLevel = GameState::viewLevel;
        const int beforeX = renderer->moveX();
        const int beforeY = renderer->moveY();
        const float beforeScale = renderer->scale();
        const QPointF local(window.width()/2,window.height()/2);
        QWheelEvent event(local,window.mapToGlobal(local.toPoint()),QPoint(),QPoint(0,delta),
            Qt::NoButton,modifiers,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(&window,&event);
        const int expected = zoom ? beforeLevel : qBound(0,beforeLevel+(delta>0 ? 1 : -1),Global::dimZ-1);
        const int afterLevel = GameState::viewLevel;
        // On the active floor, viewLevel - tile.z is zero. Moving the camera
        // would shift vertically stacked stairs by 20 * scale pixels per floor.
        const int canvasDelta = renderer->moveY()-beforeY;
        const bool scaleMatches = zoom
            ? (delta > 0 ? renderer->scale() < beforeScale : renderer->scale() > beforeScale)
            : renderer->scale() == beforeScale;
        const bool pass = afterLevel == expected && renderer->moveX() == beforeX
            && canvasDelta == 0 && scaleMatches;
        report(QString("%1 wheel delta=%2 ctrl=%3 mode=%4 level=%5->%6 canvasDelta=%7 scale=%8")
            .arg(pass ? "PASS" : "FAIL").arg(delta).arg(bool(modifiers & Qt::ControlModifier))
            .arg(zoom ? "zoom" : "layers").arg(beforeLevel).arg(afterLevel).arg(canvasDelta).arg(renderer->scale()));
    };
    QTimer::singleShot(3000,&app,[capture]() { capture("layers-before"); });
    QTimer::singleShot(5000,&app,[wheel]() {
        Global::cfg->set("toggleMouseWheel",false);
        wheel(-120,Qt::NoModifier,false);
        wheel(-120,Qt::NoModifier,false);
    });
    QTimer::singleShot(6500,&app,[capture]() { capture("layers-after-down"); });
    QTimer::singleShot(8500,&app,[folder,report,wheel,target]() {
        const QImage before(folder+"/layers-before.png");
        const QImage after(folder+"/layers-after-down.png");
        int changed = 0;
        // Include the staircase and surrounding floor two levels apart.
        const QRect patch(before.width()/2,before.height()/2-90,100,100);
        if (before.isNull() || before.size()!=after.size()) changed = patch.width()*patch.height();
        else for (int y=patch.top(); y<=patch.bottom(); ++y)
             for (int x=patch.left(); x<=patch.right(); ++x)
                 if (before.pixel(x,y)!=after.pixel(x,y)) ++changed;
        report(QString("%1 framebuffer aligned stairs two floors apart changedPixels=%2/10000")
            .arg(changed == 0 ? "PASS" : "FAIL").arg(changed));
        wheel(120,Qt::NoModifier,false);
        wheel(120,Qt::NoModifier,false);
        Global::cfg->set("toggleMouseWheel",true);
        auto& window = MainWindow::getInstance();
        for (const float scale : {0.5f,1.f,3.f}) {
            window.renderer()->setScale(scale);
            for (int rotation=0; rotation<4; ++rotation) {
                // Several events in the same frame catch stale renderer levels.
                wheel(-120,Qt::ControlModifier,false);
                wheel(-120,Qt::ControlModifier,false);
                wheel(120,Qt::ControlModifier,false);
                wheel(120,Qt::ControlModifier,false);
                window.renderer()->rotate(1);
            }
        }
        window.onUiSetViewLevel(0);
        wheel(-120,Qt::ControlModifier,false);
        window.onUiSetViewLevel(Global::dimZ-1);
        wheel(120,Qt::ControlModifier,false);
        window.onUiSetViewLevel(target.z);
        // Preserve both configurable bindings and ordinary wheel zoom.
        wheel(120,Qt::NoModifier,true);
        wheel(-120,Qt::NoModifier,true);
        Global::cfg->set("toggleMouseWheel",false);
        wheel(-120,Qt::NoModifier,false);
        wheel(120,Qt::NoModifier,false);
        wheel(120,Qt::ControlModifier,true);
        wheel(-120,Qt::ControlModifier,true);
        report("COMPLETE layer-scroll checks");
    });
    QTimer::singleShot(10000,&app,[]() {
        QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection);
    });
}
