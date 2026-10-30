#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>

#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(QStringLiteral("计算器"));

    /*
     * displayLabel 在你的 UI 中实际是 QLineEdit。
     * 将它设置为只读、右对齐，并取消焦点，
     * 防止输入框抢占键盘事件。
     */
    ui->displayLabel->setReadOnly(true);
    ui->displayLabel->setAlignment(
        Qt::AlignRight | Qt::AlignVCenter);
    ui->displayLabel->setFocusPolicy(Qt::NoFocus);
    ui->displayLabel->setClearButtonEnabled(false);

    // 连接所有按钮
    connectButtons();

    // 初始化计算器状态
    clearAll();

    // 让主窗口接收键盘事件
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::connectButtons()
{
    /*
     * 鼠标按钮和键盘事件最终都调用 processInput()，
     * 从而复用同一套计算逻辑。
     */
    auto bindButton = [this](QPushButton *button,
                             const QString &token) {
        // 防止点击按钮后按钮抢占键盘焦点
        button->setFocusPolicy(Qt::NoFocus);

        connect(button,
                &QPushButton::clicked,
                this,
                [this, token]() {
                    processInput(token);
                });
    };

    // 数字按钮：使用你的 button0～button9 命名
    bindButton(ui->button0, QStringLiteral("0"));
    bindButton(ui->button1, QStringLiteral("1"));
    bindButton(ui->button2, QStringLiteral("2"));
    bindButton(ui->button3, QStringLiteral("3"));
    bindButton(ui->button4, QStringLiteral("4"));
    bindButton(ui->button5, QStringLiteral("5"));
    bindButton(ui->button6, QStringLiteral("6"));
    bindButton(ui->button7, QStringLiteral("7"));
    bindButton(ui->button8, QStringLiteral("8"));
    bindButton(ui->button9, QStringLiteral("9"));

    // 小数点和正负号
    bindButton(ui->decimalButton, QStringLiteral("."));
    bindButton(ui->signButton, QStringLiteral("sign"));

    // 四则运算
    bindButton(ui->addButton, QStringLiteral("+"));
    bindButton(ui->subtractButton, QStringLiteral("-"));
    bindButton(ui->multiplyButton, QStringLiteral("*"));
    bindButton(ui->divideButton, QStringLiteral("/"));

    // 百分号和单操作数运算
    bindButton(ui->percentButton, QStringLiteral("percent"));
    bindButton(ui->reciprocalButton,
               QStringLiteral("reciprocal"));
    bindButton(ui->squareButton, QStringLiteral("square"));
    bindButton(ui->squareRootButton, QStringLiteral("sqrt"));

    // 清除、退格
    bindButton(ui->clearEntryButton, QStringLiteral("CE"));
    bindButton(ui->clearButton, QStringLiteral("C"));
    bindButton(ui->backspaceButton,
               QStringLiteral("Backspace"));

    // 等号
    bindButton(ui->equalsButton, QStringLiteral("="));
}

void MainWindow::processInput(const QString &token)
{
    // 数字输入
    if (token.length() == 1 && token.at(0).isDigit()) {
        inputDigit(token);
        return;
    }

    // 小数点
    if (token == QStringLiteral(".")) {
        inputDecimalPoint();
        return;
    }

    // 二元操作符
    if (token == QStringLiteral("+")
        || token == QStringLiteral("-")
        || token == QStringLiteral("*")
        || token == QStringLiteral("/")) {
        inputOperator(token);
        return;
    }

    // 等号
    if (token == QStringLiteral("=")) {
        calculateResult();
        return;
    }

    // 百分号
    if (token == QStringLiteral("percent")) {
        applyPercent();
        return;
    }

    // 倒数
    if (token == QStringLiteral("reciprocal")) {
        applyReciprocal();
        return;
    }

    // 平方
    if (token == QStringLiteral("square")) {
        applySquare();
        return;
    }

    // 平方根
    if (token == QStringLiteral("sqrt")) {
        applySquareRoot();
        return;
    }

    // 正负号
    if (token == QStringLiteral("sign")) {
        toggleSign();
        return;
    }

    // 清除当前输入
    if (token == QStringLiteral("CE")) {
        clearEntry();
        return;
    }

    // 全部清除
    if (token == QStringLiteral("C")) {
        clearAll();
        return;
    }

    // 退格
    if (token == QStringLiteral("Backspace")) {
        backspace();
    }
}

void MainWindow::inputDigit(const QString &digit)
{
    // 错误状态下输入数字，自动开始一次新计算
    if (errorState) {
        clearAll();
    }

    /*
     * 计算结果显示后直接输入数字，
     * 应开始一次新计算，而不是追加到结果后面。
     */
    if (resultShown && pendingOperator.isEmpty()) {
        clearAll();
    }

    if (waitingForOperand) {
        // 正在等待新操作数，数字替换原显示内容
        currentInput = digit;
        waitingForOperand = false;
    } else if (currentInput == QStringLiteral("0")) {
        // 避免出现 00012
        currentInput = digit;
    } else if (currentInput == QStringLiteral("-0")) {
        currentInput = QStringLiteral("-") + digit;
    } else {
        /*
         * 最多允许输入16位有效数字。
         * 小数点和负号不计入长度。
         */
        QString effectiveText = currentInput;
        effectiveText.remove('-');
        effectiveText.remove('.');

        if (effectiveText.length() < 16) {
            currentInput.append(digit);
        }
    }

    resultShown = false;
    updateDisplay();
}

void MainWindow::inputDecimalPoint()
{
    if (errorState) {
        clearAll();
    }

    // 结果后输入小数点，开始输入新的小数
    if (resultShown && pendingOperator.isEmpty()) {
        clearAll();
    }

    if (waitingForOperand) {
        currentInput = QStringLiteral("0.");
        waitingForOperand = false;
    } else if (!currentInput.contains('.')
               && !currentInput.contains(
                   'e', Qt::CaseInsensitive)) {
        /*
         * 当前操作数没有小数点时才允许添加，
         * 从而避免输入 1.2.3。
         */
        currentInput.append('.');
    }

    resultShown = false;
    updateDisplay();
}

void MainWindow::inputOperator(const QString &op)
{
    if (errorState) {
        return;
    }

    if (pendingOperator.isEmpty()) {
        /*
         * 第一次输入操作符，
         * 将当前数字保存为第一个操作数。
         */
        accumulator = currentInput.toDouble();
    } else if (!waitingForOperand) {
        /*
         * 已经存在操作符，并且第二个操作数输入完成，
         * 先计算上一次运算，实现连续运算。
         *
         * 例如：2 + 3 + 4 =
         * 第二次按 + 时先计算 2 + 3。
         */
        const double rightOperand = currentInput.toDouble();

        if (!performCalculation(rightOperand,
                                pendingOperator)) {
            return;
        }

        currentInput = formatNumber(accumulator);
        updateDisplay();
    }

    /*
     * 连续输入操作符时，新的操作符覆盖旧操作符。
     *
     * 例如：8 + × 2 =
     * 最终按照 8 × 2 计算。
     */
    pendingOperator = op;
    waitingForOperand = true;
    resultShown = false;

    // 输入新操作符后取消上一次连续等号记录
    lastOperator.clear();
    lastOperand = 0.0;
}

void MainWindow::calculateResult()
{
    if (errorState) {
        return;
    }

    /*
     * 正常情况：
     * 已有操作符，并且第二个操作数已经输入。
     */
    if (!pendingOperator.isEmpty()
        && !waitingForOperand) {
        const double rightOperand =
            currentInput.toDouble();

        const QString op = pendingOperator;

        // 保存操作，用于连续按等号
        lastOperator = op;
        lastOperand = rightOperand;

        if (!performCalculation(rightOperand, op)) {
            return;
        }

        currentInput = formatNumber(accumulator);
        pendingOperator.clear();

        waitingForOperand = true;
        resultShown = true;

        updateDisplay();
        return;
    }

    /*
     * 连续按等号。
     *
     * 例如：
     * 2 + 3 = = =
     *
     * 结果依次为：
     * 5、8、11
     */
    if (resultShown && !lastOperator.isEmpty()) {
        accumulator = currentInput.toDouble();

        if (!performCalculation(lastOperand,
                                lastOperator)) {
            return;
        }

        currentInput = formatNumber(accumulator);
        waitingForOperand = true;
        resultShown = true;

        updateDisplay();
    }
}

bool MainWindow::performCalculation(
    double rightOperand,
    const QString &op)
{
    if (op == QStringLiteral("+")) {
        accumulator += rightOperand;
    } else if (op == QStringLiteral("-")) {
        accumulator -= rightOperand;
    } else if (op == QStringLiteral("*")) {
        accumulator *= rightOperand;
    } else if (op == QStringLiteral("/")) {
        if (rightOperand == 0.0) {
            showError(QStringLiteral("除数不能为零"));
            return false;
        }

        accumulator /= rightOperand;
    }

    // 判断计算结果是否为无穷大或非法数字
    if (!std::isfinite(accumulator)) {
        showError(QStringLiteral("计算结果无效"));
        return false;
    }

    return true;
}

void MainWindow::applyPercent()
{
    if (errorState) {
        return;
    }

    /*
     * 如果刚输入操作符但尚未输入第二个操作数，
     * 将当前输入视为 0。
     */
    if (waitingForOperand
        && !pendingOperator.isEmpty()) {
        currentInput = QStringLiteral("0");
    }

    const double value = currentInput.toDouble();
    double result = 0.0;

    /*
     * 模拟标准计算器的百分号逻辑：
     *
     * 200 + 10% = 220
     * 200 - 10% = 180
     * 200 × 10% = 20
     * 200 ÷ 10% = 2000
     */
    if (pendingOperator == QStringLiteral("+")
        || pendingOperator == QStringLiteral("-")) {
        result = accumulator * value / 100.0;
    } else {
        result = value / 100.0;
    }

    if (!std::isfinite(result)) {
        showError(QStringLiteral("计算结果无效"));
        return;
    }

    currentInput = formatNumber(result);
    waitingForOperand = false;
    resultShown = false;

    updateDisplay();
}

void MainWindow::applyReciprocal()
{
    if (errorState) {
        return;
    }

    if (waitingForOperand
        && !pendingOperator.isEmpty()) {
        currentInput = QStringLiteral("0");
    }

    const double value = currentInput.toDouble();

    if (value == 0.0) {
        showError(QStringLiteral("不能除以零"));
        return;
    }

    const double result = 1.0 / value;

    if (!std::isfinite(result)) {
        showError(QStringLiteral("计算结果无效"));
        return;
    }

    currentInput = formatNumber(result);
    waitingForOperand = false;

    updateDisplay();
}

void MainWindow::applySquare()
{
    if (errorState) {
        return;
    }

    if (waitingForOperand
        && !pendingOperator.isEmpty()) {
        currentInput = QStringLiteral("0");
    }

    const double value = currentInput.toDouble();
    const double result = value * value;

    if (!std::isfinite(result)) {
        showError(QStringLiteral("计算结果溢出"));
        return;
    }

    currentInput = formatNumber(result);
    waitingForOperand = false;

    updateDisplay();
}

void MainWindow::applySquareRoot()
{
    if (errorState) {
        return;
    }

    if (waitingForOperand
        && !pendingOperator.isEmpty()) {
        currentInput = QStringLiteral("0");
    }

    const double value = currentInput.toDouble();

    if (value < 0.0) {
        showError(QStringLiteral("无效输入"));
        return;
    }

    const double result = std::sqrt(value);

    if (!std::isfinite(result)) {
        showError(QStringLiteral("计算结果无效"));
        return;
    }

    currentInput = formatNumber(result);
    waitingForOperand = false;

    updateDisplay();
}

void MainWindow::toggleSign()
{
    if (errorState) {
        return;
    }

    /*
     * 如果刚输入操作符，此时按正负号，
     * 从 0 开始输入负数。
     */
    if (waitingForOperand
        && !pendingOperator.isEmpty()) {
        currentInput = QStringLiteral("-0");
        waitingForOperand = false;
        resultShown = false;
        updateDisplay();
        return;
    }

    const double value = currentInput.toDouble();

    // 避免普通情况下显示 -0
    if (value == 0.0) {
        currentInput = QStringLiteral("0");
    } else if (currentInput.startsWith('-')) {
        currentInput.remove(0, 1);
    } else {
        currentInput.prepend('-');
    }

    waitingForOperand = false;
    resultShown = false;

    updateDisplay();
}

void MainWindow::clearEntry()
{
    /*
     * CE 只清除当前输入，
     * 不清除第一个操作数和等待执行的操作符。
     *
     * 例如：
     * 5 + 123 CE 2 =
     * 结果为 7。
     */
    if (errorState) {
        clearAll();
        return;
    }

    currentInput = QStringLiteral("0");
    waitingForOperand = true;
    resultShown = false;

    updateDisplay();
}

void MainWindow::clearAll()
{
    /*
     * C 清除计算器中的全部状态。
     */
    currentInput = QStringLiteral("0");
    accumulator = 0.0;

    pendingOperator.clear();
    lastOperator.clear();
    lastOperand = 0.0;

    waitingForOperand = true;
    resultShown = false;
    errorState = false;

    updateDisplay();
}

void MainWindow::backspace()
{
    if (errorState) {
        clearAll();
        return;
    }

    /*
     * 已经显示结果或正在等待新操作数时，
     * 不执行退格。
     */
    if (resultShown || waitingForOperand) {
        return;
    }

    if (currentInput.length() <= 1) {
        currentInput = QStringLiteral("0");
    } else {
        currentInput.chop(1);

        if (currentInput.isEmpty()
            || currentInput == QStringLiteral("-")) {
            currentInput = QStringLiteral("0");
        }
    }

    updateDisplay();
}

void MainWindow::showError(const QString &message)
{
    errorState = true;

    currentInput = QStringLiteral("0");
    accumulator = 0.0;

    pendingOperator.clear();
    lastOperator.clear();
    lastOperand = 0.0;

    waitingForOperand = true;
    resultShown = false;

    ui->displayLabel->setText(message);
}

void MainWindow::updateDisplay()
{
    if (errorState) {
        return;
    }

    ui->displayLabel->setText(currentInput);

    /*
     * 根据显示内容长度自动调整字体大小，
     * 防止数字过长时无法完整显示。
     */
    QFont font = ui->displayLabel->font();

    if (currentInput.length() <= 12) {
        font.setPointSize(28);
    } else if (currentInput.length() <= 16) {
        font.setPointSize(22);
    } else {
        font.setPointSize(17);
    }

    ui->displayLabel->setFont(font);
}

QString MainWindow::formatNumber(double value) const
{
    // 避免显示 -0
    if (value == 0.0) {
        return QStringLiteral("0");
    }

    /*
     * g 格式会自动去除小数末尾多余的 0，
     * 数值很大或很小时会自动使用科学计数法。
     */
    return QString::number(value, 'g', 15);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QString token;

    // 主键盘和数字小键盘的0～9都可以处理
    if (event->key() >= Qt::Key_0
        && event->key() <= Qt::Key_9) {
        token = QString::number(
            event->key() - Qt::Key_0);
    } else {
        switch (event->key()) {
        case Qt::Key_Plus:
            token = QStringLiteral("+");
            break;

        case Qt::Key_Minus:
            token = QStringLiteral("-");
            break;

        case Qt::Key_Asterisk:
            token = QStringLiteral("*");
            break;

        case Qt::Key_Slash:
            token = QStringLiteral("/");
            break;

        case Qt::Key_Percent:
            token = QStringLiteral("percent");
            break;

        case Qt::Key_Period:
        case Qt::Key_Comma:
            token = QStringLiteral(".");
            break;

        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Equal:
            token = QStringLiteral("=");
            break;

        case Qt::Key_Backspace:
            token = QStringLiteral("Backspace");
            break;

        case Qt::Key_Delete:
            token = QStringLiteral("CE");
            break;

        case Qt::Key_Escape:
            token = QStringLiteral("C");
            break;

        default:
            // 不是计算器支持的按键，交给父类处理
            QMainWindow::keyPressEvent(event);
            return;
        }
    }

    processInput(token);
    event->accept();
}


