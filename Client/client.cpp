#include "client.h"
#include "ui_client.h"
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <windows.h>
#include <QMouseEvent>
#include <QDebug>
#include "addfriend.h"
#include "frienditem.h"
#include <QListWidget>
#include <QToolTip>
#include "stringtool.h"
#include <QShortcut>
#include "sendtextedit.h"
#include "selfinfowidget.h"


Client::Client(SelfInfo info ,TcpClient* tcp,QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Client)
{
    ui->setupUi(this);
    InitUI();
    t = tcp;
    selfInfo.name = info.name;
    selfInfo.account=info.account;
    selfInfo.password=info.password;
    messagesListWidget = new ChatListWidget;
    friendsListWidget = new ChatListWidget;
    groupsListWidget = new ChatListWidget;


    connect(t,&TcpClient::CallClient,this,&Client::ClientMsgHandler);
    connect(ui->textEdit_send,&SendTextEdit::keyPressEnter,this,&Client::on_pushBtn_send_clicked);
    connect(friendsListWidget,&ChatListWidget::itemDoubleClicked,this,&Client::on_listWidget_info_itemClicked);


    ui->stackedWidget_list->addWidget(messagesListWidget);
    ui->stackedWidget_list->addWidget(friendsListWidget);
    ui->stackedWidget_list->addWidget(groupsListWidget);
    //RefreshFriendList();
}

Client::~Client()
{
    delete ui;
}

void Client::RefreshFriendList()
{
    json msg;
    msg.insert("cmd",cmd_friend_list);
    msg.insert("account",QString("%1").arg(selfInfo.account));
    t->SendMsg(msg);
}

void Client::RefreshGroupList()
{
    json msg;
    msg.insert("cmd",cmd_group_list);
    msg.insert("account",QString("%1").arg(selfInfo.account));
    t->SendMsg(msg);
}

void Client::InitUI()
{
    this->setWindowTitle("OurChat");
    //    int width = this->width()-10;
    //    int height = this->height()-10;
    //    ui->centerWidget->setGeometry(5,5,width,height);
    //    ui->centerWidget->setStyleSheet("QWidget#centerWidget{ border-radius:4px; background:rgba(255,255,255,1); }");
    this->setWindowFlags(Qt::FramelessWindowHint);          //去掉标题栏无边框
    this->setAttribute(Qt::WA_TranslucentBackground,true);
    //实例阴影shadow
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    //设置阴影距离
    shadow->setOffset(0, 0);
    //设置阴影颜色
    shadow->setColor(QColor(39,40,43,100));
    //设置阴影圆角
    shadow->setBlurRadius(10);
    //给嵌套QWidget设置阴影
    ui->centerWidget->setGraphicsEffect(shadow);


    m_isfull = false;
}

void Client::mousePressEvent(QMouseEvent *event)
{
    mouseWindowTopLeft = event->pos();
}

void Client::mouseMoveEvent(QMouseEvent *event)
{
    //窗口移动
    if (event->buttons() & Qt::LeftButton)
    {
        mouseDeskTopLeft = event->globalPos();
        windowDeskTopLeft = mouseDeskTopLeft - mouseWindowTopLeft;  //矢量计算
        this->move(windowDeskTopLeft);     //移动到目的地
    }
}

void Client::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if(m_isfull){
        //取消全屏
        m_isfull = false;
        ui->centerWidget->setGeometry(m_rect);

        ui->centerWidget->move(QApplication::desktop()->screen()->rect().center() - ui->centerWidget->rect().center());
    }
    else {
        m_isfull = true;
        m_rect = ui->centerWidget->rect();
        setGeometry(QGuiApplication::primaryScreen()->availableGeometry()); // 不包含windows任务栏区域
        ui->centerWidget->setGeometry(this->rect());
    }
}


void Client::on_pushBtn_max_clicked()
{
    //    this->showFullScreen(); //全屏
    if(m_isfull){
        //取消全屏
        m_isfull = false;
        ui->centerWidget->setGeometry(640,480,m_rect.width(),m_rect.height());
        ui->centerWidget->move(QApplication::desktop()->screen()->rect().center() - ui->centerWidget->rect().center());
    }
    else {
        m_isfull = true;
        m_rect = ui->centerWidget->rect();
        setGeometry(QGuiApplication::primaryScreen()->availableGeometry()); // 不包含windows任务栏区域
        ui->centerWidget->setGeometry(this->rect());
    }
    //    ui->centerWidget->showMaximized();
    //    this->showMaximized();
}

void Client::on_pushBtn_close_clicked()
{
    json msg ={ {"cmd",cmd_logout}};
    t->SendMsg(msg);
    this->close();
}

void Client::on_pushBtn_hide_clicked()
{
    QWidget* pWindow = this->window();
    if(pWindow->isTopLevel())
        pWindow->showMinimized();
}

void Client::on_pushButton_addFriend_clicked()
{
    AddFriend *add = new AddFriend(t);
    add->show();
}

void Client::on_pushBtn_refresh_clicked()
{
    switch(curListWidgetIndex) {
        case 0:return;
        case 1:RefreshFriendList();
        case 2:RefreshGroupList();
        default:
            return;
    }
}

void Client::ClientMsgHandler(json msg)
{
    qDebug() << "ClientMsgHandler" << endl;
    int cmd = msg["cmd"].toInt();
    switch(cmd) {
        case cmd_friend_list: {
            friendsListWidget->clear();
            friendMap.clear();
            friendItemMap.clear();
            QJsonArray list = msg["msglist"].toArray();
            for (int i = 0; i < list.size(); i++) {
                FriendInfo info;
                json obj = list[i].toObject();
                info.name = obj["name"].toString();
                info.account = obj["account"].toString().toInt();
                info.sig = obj["sig"].toString();
                info.isOnline = obj["isOnline"].toBool();
                if (info.sig.isEmpty())
                    info.sig = "这家伙很高冷，啥也不想说";

                FriendItem *item = new FriendItem(info);

                QListWidgetItem *listItem = new QListWidgetItem(friendsListWidget);
                listItem->setSizeHint(QSize(260, 85));
                friendsListWidget->addItem(listItem);
                friendsListWidget->setItemWidget(listItem, item);

                friendMap.insert(item->account(), info);
                friendItemMap.insert(info.account, item);
            }
            break;
        }
        case cmd_friend_chat: {
            int account = msg["sender"].toString().toInt();
            ChatWindow *chatWindow;
            if (chatMap.find(account) == chatMap.end())  //账号对应的聊天窗口不存在
            {
                if (friendMap.find(account) != friendMap.end()) {
                    FriendInfo info = friendMap[account];
                    chatWindow = new ChatWindow(info);

                    ui->stackedWidget->addWidget(chatWindow);
                    chatMap.insert(account, chatWindow);
                } else {
                    //单向好友
                    return;
                }
            } else {
                chatWindow = chatMap.value(account);
                // ui->stackedWidget->setCurrentWidget(chatMap.value(account));
            }
            QString pushMsg = StringTool::MergePushMsg(currentDateTime, chatWindow->GetFriendInfo()->name,
                                                       msg["sendmsg"].toString());
            chatWindow->pushMsg(pushMsg, 1);
            FriendItem *item = friendItemMap.value(account);
            if (account != curChatAccount) {
                item->NewMsgPlusOne();
            }
            break;
        }
    }
}

void Client::on_listWidget_info_itemClicked(QListWidgetItem *item)
{
    FriendItem* friendItem = qobject_cast<FriendItem*>(friendsListWidget->itemWidget(item));
    int account = friendItem->account();

    if(chatMap.find(account) == chatMap.end())  //账号对应的聊天窗口不存在
    {
        FriendInfo info{account,friendItem->getName()};
        ChatWindow* chatWindow = new ChatWindow(info);
        ui->stackedWidget->addWidget(chatWindow);
        ui->stackedWidget->setCurrentWidget(chatWindow);
        chatMap.insert(account,chatWindow);
    }
    else
    {
        ui->stackedWidget->setCurrentWidget(chatMap.value(account));
    }
    friendItem->SetNewMsgNum(0);
    curChatAccount = account;
    ui->label_info->setText(friendItem->GetInfo().name);
}

void Client::on_pushBtn_send_clicked()
{
    QString sendText = ui->textEdit_send->toPlainText();
    if( sendText== "")
    {
        QToolTip::showText(ui->pushBtn_send->mapToGlobal(QPoint(0, -50)), "发送的消息不能为空", ui->pushButton);
        return;
    }
    if(friendsListWidget->currentRow()<0)
    {
        QToolTip::showText(ui->pushBtn_send->mapToGlobal(QPoint(0, -50)), "选择的好友不能为空", ui->pushButton);
        return;
    }
    ChatWindow* chatWindow = qobject_cast<ChatWindow*>(ui->stackedWidget->currentWidget());
    int account = chatWindow->GetFriendInfo()->account;
    json msg={{"cmd",cmd_friend_chat},{"account",QString("%1").arg(account)},{"sendmsg",sendText},{"sender",QString("%1").arg(selfInfo.account)}};
    t->SendMsg(msg);
    ui->textEdit_send->clear();
    QString pushMsg = StringTool::MergePushMsg(currentDateTime,selfInfo.name,sendText);
    chatWindow->pushMsg(pushMsg,0);

}


void Client::on_pushBtn_refresh_2_clicked()
{
    t->waitForReadyRead(200);
}

void Client::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        on_pushBtn_send_clicked();
    }
}


void Client::on_pushButton_emoj_3_clicked()
{
    SelfInfoWidget* w = new SelfInfoWidget;
    w->show();
}

void Client::on_pushButton_msg_list_clicked()
{
    curListWidgetIndex = 0;
    ui->stackedWidget_list->setCurrentWidget(messagesListWidget);
}

void Client::on_pushButton_friend_list_clicked()
{
    curListWidgetIndex = 1;
    ui->stackedWidget_list->setCurrentWidget(friendsListWidget);
}

void Client::on_pushButton_group_list_clicked()
{
    curListWidgetIndex = 2;
    ui->stackedWidget_list->setCurrentWidget(groupsListWidget);
}
