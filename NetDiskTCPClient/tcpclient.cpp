#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QByteArray>
#include <QDebug> // 调试
#include <QMessageBox> // 消息提示框
#include <QHostAddress>

#include "protocol.h"

TcpClient::TcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TcpClient)
{
    ui->setupUi(this);
    loadConfig(); // 调用访问配置信息

    // 绑定连接服务器信号处理的槽函数
    connect(&m_tcpSocket, SIGNAL(connected()), // 信号发送方（Socket变量），发送信号类型
            this, SLOT(showConnect())); // 信号处理方，用以处理的槽函数
    // 绑定处理服务器响应消息的槽函数
    connect(&m_tcpSocket, SIGNAL(readyRead()), // 信号发送方（Socket变量），发送信号类型
            this, SLOT(receiveMsg())); // 信号处理方，用以处理的槽函数

    // 连接服务器
    m_tcpSocket.connectToHost(QHostAddress(m_strIP), m_usPort);
}

TcpClient::~TcpClient()
{
    delete ui;
}

// 头文件中加载配置文件函数的定义实现
void TcpClient::loadConfig()
{
    QFile file(":/client.config"); // 文件对象，读取资源文件 ':' + "前缀" + "文件名"
    if(file.open(QIODevice::ReadOnly)) // file.open() 参数：打开方式：只读（注意，这里只读是写在QIODevice下的枚举，所以调用要声明命名空间） 返回true则打开成功
    {
        QByteArray baData = file.readAll(); // 读出所有数据，返回字节数组QByteArray
        QString strData = baData.toStdString().c_str(); // 转换为字符串 注意std::string不能自动转为QString，还需转为char*
        file.close();

        strData.replace("\r\n", " "); // 替换IP地址与端口号之间\r\n
        QStringList strList = strData.split(" ");
        m_strIP = strList.at(0);
        m_usPort = strList.at(1).toUShort(); // 无符号短整型
        qDebug() << "IP:" << m_strIP << " port:" << m_usPort; // 打印结果
    }
    else // 文件打开失败则弹出提示窗口
    {
        QMessageBox::critical(this, "open config", "open config failed"); // 严重
    }
}

TcpClient &TcpClient::getInstance()
{
    static TcpClient instance;
    return instance;
}

// 头文件中检测服务器是否连接成功函数实现
void TcpClient::showConnect()
{
    // QMessageBox::information(this, "连接服务器", "连接服务器成功");
    qDebug() << "连接服务器成功";
}

void TcpClient::receiveMsg()
{
    // qDebug() << m_tcpSocket.bytesAvailable(); // 输出接收到的数据大小
    uint uiPDULen = 0;
    m_tcpSocket.read((char*)&uiPDULen, sizeof(uint)); // 先读取uint大小的数据，首个uint正是总数据大小
    uint uiMsgLen = uiPDULen - sizeof(PDU); // 实际消息大小，sizeof(PDU)只会计算结构体大小，而不是分配的大小
    PDU *pdu = mkPDU(uiMsgLen);
    m_tcpSocket.read((char*)pdu + sizeof(uint), uiPDULen - sizeof(uint)); // 接收剩余部分数据（第一个uint已读取）
    // qDebug() << pdu -> uiMsgType << ' ' << (char*)pdu -> caMsg; // 输出

    // 根据不同消息类型，执行不同操作
    switch(pdu -> uiMsgType)
    {
    case ENUM_MSG_TYPE_REGIST_RESPOND: // 注册响应
    {
        if(0 == strcmp(pdu -> caData, REGIST_OK))
        {
            QMessageBox::information(this, "注册", REGIST_OK);
        }
        else if(0 == strcmp(pdu -> caData, REGIST_FAILED))
        {
            QMessageBox::warning(this, "注册", REGIST_FAILED);
        }
        break;
    }
    case ENUM_MSG_TYPE_LOGIN_RESPOND: // 登录响应
    {
        if(0 == strcmp(pdu -> caData, LOGIN_OK))
        {
            // QMessageBox::information(this, "登录", LOGIN_OK);
            char caName[32] = {'\0'};
            strncpy(caName, pdu -> caData + 32, 32); // 设置已登录用户名
            m_strName = caName;
            qDebug() << "用户已登录：" << caName << " strName：" << m_strName;
            // 登录跳转
            OperateWidget::getInstance().show(); // 显示主操作页面
            // 默认请求一次好友列表
            OperateWidget::getInstance().getPFriend() -> flushFriendList();

            this -> hide(); // 隐藏登陆页面
        }
        else if(0 == strcmp(pdu -> caData, LOGIN_FAILED))
        {
            QMessageBox::warning(this, "登录", LOGIN_FAILED);
        }
        break;
    }
    case ENUM_MSG_TYPE_ONLINE_USERS_RESPOND: // 查询所有在线用户响应
    {
        OperateWidget::getInstance().getPFriend() -> setOnlineUsers(pdu);
        break;
    }
    case ENUM_MSG_TYPE_SEARCH_USER_RESPOND: // 查找用户响应
    {
        if(0 == strcmp(SEARCH_USER_OK, pdu -> caData))
        {
            QMessageBox::information(this, "查找", OperateWidget::getInstance().getPFriend()->getStrSearchName() + SEARCH_USER_OK);
        }
        else if(0 == strcmp(SEARCH_USER_OFFLINE, pdu -> caData))
        {
            QMessageBox::information(this, "查找", OperateWidget::getInstance().getPFriend()->getStrSearchName() + SEARCH_USER_OFFLINE);
        }
        else if(0 == strcmp(SEARCH_USER_EMPTY, pdu -> caData))
        {
            QMessageBox::warning(this, "查找", OperateWidget::getInstance().getPFriend()->getStrSearchName() + SEARCH_USER_EMPTY);
        }
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_RESPOND: // 好友请求回复消息
    {
        QMessageBox::information(this, "添加好友", pdu -> caData);
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST: // 处理服务器转发过来的好友申请消息
    {
        char sourceName[32]; // 获取发送方用户名
        strncpy(sourceName, pdu -> caData + 32, 32);
        int ret = QMessageBox::information(this, "好友申请", QString("%1 想添加您为好友，是否同意？").arg(sourceName),
                                 QMessageBox::Yes, QMessageBox::No); // 后面两个参数是为QMessage默认支持两个按钮来设置枚举值
        PDU* resPdu = mkPDU(0);

        strncpy(resPdu -> caData, pdu -> caData, 32); // 被加好友者用户名
        strncpy(resPdu -> caData + 32, pdu -> caData + 32, 32); // 加好友者用户名
        // qDebug() << "同意加好友吗？" << resPdu -> caData << " " << resPdu -> caData + 32;
        if(ret == QMessageBox::Yes) // 同意加好友
        {
            resPdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_AGREE;
        }
        else
        {
            resPdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REJECT;
        }
        m_tcpSocket.write((char*)resPdu, resPdu -> uiPDULen); // 发送给服务器消息，由服务器写入数据库并转发给用户

        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE: // 对方同意加好友
    {
        QMessageBox::information(this, "添加好友", QString("%1 已同意您的好友申请！").arg(pdu -> caData));
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REJECT: // 对方拒绝加好友
    {
        QMessageBox::information(this, "添加好友", QString("%1 已拒绝您的好友申请！").arg(pdu -> caData));
        break;
    }
    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND: // 刷新好友响应
    {
        OperateWidget::getInstance().getPFriend()->updateFriendList(pdu);
        break;
    }
    default:
        break;
    }

    // 释放空间
    free(pdu);
    pdu = NULL;
}

void TcpClient::on_login_pb_clicked()
{
    QString strName = ui -> name_le -> text();
    QString strPwd = ui -> pwd_le -> text();
    // 合理性判断
    if(!strName.isEmpty() && !strPwd.isEmpty())
    {
        PDU *pdu = mkPDU(0); // 实际消息体积为0
        pdu -> uiMsgType = ENUM_MSG_TYPE_LOGIN_REQUEST; // 设置为登录请求消息类型
        // 拷贝用户名和密码信息到caData
        memcpy(pdu -> caData, strName.toStdString().c_str(), 32); // 由于数据库设定的32位，所以最多只拷贝前32位
        memcpy(pdu -> caData + 32, strPwd.toStdString().c_str(), 32);
        // qDebug() << pdu -> uiMsgType << " " << pdu -> caData << " " << pdu -> caData + 32;
        m_tcpSocket.write((char*)pdu, pdu -> uiPDULen); // 发送消息

        // 释放空间
        free(pdu);
        pdu = NULL;
    }
    else
    {
        QMessageBox::critical(this, "登录", "登录失败：用户名或密码为空！");
    }
}

void TcpClient::on_regist_pb_clicked()
{
    QString strName = ui -> name_le -> text(); // 获取用户名和密码
    QString strPwd = ui -> pwd_le -> text();
    // 合理性判断
    if(!strName.isEmpty() && !strPwd.isEmpty())
    {
        // 注册信息用户名和密码将通过caData[64]传输
        PDU *pdu = mkPDU(0); // 实际消息体积为0
        pdu -> uiMsgType = ENUM_MSG_TYPE_REGIST_REQUEST; // 设置为注册请求消息类型
        // 拷贝用户名和密码信息到caData
        memcpy(pdu -> caData, strName.toStdString().c_str(), 32); // 由于数据库设定的32位，所以最多只拷贝前32位
        memcpy(pdu -> caData + 32, strPwd.toStdString().c_str(), 32);
        // qDebug() << pdu -> uiMsgType << " " << pdu -> caData << " " << pdu -> caData + 32;
        m_tcpSocket.write((char*)pdu, pdu -> uiPDULen); // 发送消息

        // 释放空间
        free(pdu);
        pdu = NULL;
    }
    else
    {
        QMessageBox::critical(this, "注册", "注册失败：用户名或密码为空！");
    }
}

void TcpClient::on_logout_pb_clicked()
{

}

QString TcpClient::getStrName() const
{
    return m_strName;
}

void TcpClient::setStrName(const QString &strName)
{
    m_strName = strName;
}

QTcpSocket& TcpClient::getTcpSocket()
{
    return m_tcpSocket;
}
