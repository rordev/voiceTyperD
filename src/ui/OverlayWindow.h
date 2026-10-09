#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

namespace vt {

// Small always-on-top, non-focus-stealing overlay shown while recording and
// while the take is processed. Recording: a pulsing red dot, a status line,
// the recording time and a live input-level meter. Processing: an amber dot,
// the step's status and how long that step has been running. It deliberately
// never takes focus so the user's target text field stays active.
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget* parent = nullptr);

    // Shows the recording view; time and level come from the setters below.
    void showOverlay();
    // Switches to the processing view for a new step (transcription, the LLM
    // request): `status`, with a timer counting from zero.
    void showProcessing(const QString& status);
    void hideOverlay();

public slots:
    void setStatus(const QString& status);
    void setElapsedSeconds(double seconds);
    void setLevel(double level); // 0..1

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void positionInCorner();

    QString status_ = tr("Recording");
    double elapsed_ = 0.0;
    double level_ = 0.0;
    bool pulseOn_ = true;
    QTimer pulseTimer_;
    bool processing_ = false;
    QElapsedTimer stepClock_; // started by showProcessing()
    QTimer stepTimer_;        // refreshes the step time while processing
};

} // namespace vt
