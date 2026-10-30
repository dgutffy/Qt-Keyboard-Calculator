#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QKeyEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Ui::MainWindow *ui;

    // 当前显示的数字
    QString currentInput = QStringLiteral("0");

    // 第一个操作数或连续运算的中间结果
    double accumulator = 0.0;

    // 当前等待执行的二元运算符
    QString pendingOperator;

    // 用于连续按等号
    QString lastOperator;
    double lastOperand = 0.0;

    // 是否正在等待输入新的操作数
    bool waitingForOperand = true;

    // 是否刚刚显示了计算结果
    bool resultShown = false;

    // 是否处于错误状态
    bool errorState = false;

    void connectButtons();
    void processInput(const QString &token);

    void inputDigit(const QString &digit);
    void inputDecimalPoint();
    void inputOperator(const QString &op);

    void calculateResult();
    bool performCalculation(double rightOperand, const QString &op);

    void applyPercent();
    void applyReciprocal();
    void applySquare();
    void applySquareRoot();
    void toggleSign();

    void clearEntry();
    void clearAll();
    void backspace();

    void updateDisplay();
    void showError(const QString &message);

    QString formatNumber(double value) const;
};

#endif // MAINWINDOW_H
