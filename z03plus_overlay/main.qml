import QtQuick 2.10
// Z03Ultra battery patch: initialize percentage text and refresh it periodically
import QtQuick.Window 2.10
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
import QtWebView 1.1
import QtWebChannel 1.0
import QtWebEngine 1.4
import "./ZybQmlFiles/Style"
import "./ZybQmlFiles/ZybControls"
import "./ZybQmlFiles/ZybVirturlKeyBoard"
import "./ZybQmlFiles/Funcs/UILogics.js" as UILogic
import QtQuick.Templates 2.3
import QtQuick.Controls.impl 2.3
import DevConnection.StatusDefine 1.0
import DataManager.PageModeDefine 1.0
import RemoteDataManager.PermissionDefine 1.0
import BtManager.BtStatusDefine 1.0
import "./ZybQmlFiles/Global/"
import "./ZybQmlFiles/ZybLayoutViews/"
import "./ZybQmlFiles/ZybSet"
import "./ZybQmlFiles/ZybPages"
import com.zyb.PingDefine 1.0
import DevStateMonitor.DevStateDefine 1.0

Rectangle
{
    x:0
    y:0
    width: Style.gWidth
    height: 480


    Rectangle {
        id: main
        //anchors.fill: parent
        x:0
        y: Style.highVersion ? 120:106
        width: Style.gWidth
        height: Style.gHeight
        color: Style.mainbgColor
        rotation: devManager.isLeftHand ?180:0

        property bool btimeout:false
        property bool bEnterSpeechFormKey : false
        property bool bEnterCameraFormKey : false
        property bool bEnterPicFormKey : false
        property bool bEnterPhoneFormOut : false
        property bool bWarnLow20Elec : false
        property bool bWarnLow10Elec : false
        property bool bReady : false

        //对话框实用工具
        DlgUtils{id: dlgUtils}

        //wifi通用工具
        WifiUtils{id: wifiUtils}

        //用于UI测试
//                Flickable{
//                    anchors.fill: parent
//                    ScrollBar.horizontal: ScrollBar {visible: false } //创建一个竖向的滚动条
//                    contentWidth: newm.width
//                    z:100
//                    Image {
//                        id: newm
//                        visible: true
//                        anchors.top: parent.top
//                        anchors.left: parent.left
//                        state: "p1"
//                        source: "file://home/userdata/" + Segmentation.getcurUi();
//                    }
//                    Rectangle{
//                        anchors.top: parent.top
//                        anchors.left: parent.left
//                        width: 100
//                        height: 100
//                        color: "transparent"
//                        MouseArea{
//                            anchors.fill: parent
//                            onClicked: {
//                                console.log(newm.source)
//                                newm.source = "file://home/userdata/" + Segmentation.getPreUi();
//                            }
//                        }
//                    }

//                    Rectangle{
//                        anchors.top: parent.top
//                        anchors.right: parent.right
//                        width: 100
//                        height: 100
//                        color: "transparent"
//                        MouseArea{
//                            anchors.fill: parent
//                            onClicked: {
//                                console.log(newm.source)
//                                newm.source = "file://home/userdata/" + Segmentation.getNextUi();
//                            }
//                        }
//                    }
//                }


        Loader {
            id: sceneLoader

            onLoaded: {
                if(item.pageType === "wordpage"){
                    if(scanManager.isScanUp()){
                        item.pageState = 0;
                    }
                    else{
                        item.pageState = 1;
                    }
                }
            }

            Component.onCompleted: {
            }
        }

        ZybScanWordView2{
            id:wordView
            anchors.fill: parent
            visible: false
            z:100
        }

        property  var res : new Object;
        function tidModeClicked()
        {
            if(devManager.getScanForWhat()===0)
            {
                devManager.setScanForWhat(1);
                devManager.saveConfigFile();
                res["mode"] = "tid"
                pingManager.addPingData(PingDefine.PING_EVENT_NAVI_SWITCH_MODE,res);
            }

            //            UILogic.openTipPage("question");

            naviPanel.resetModel();
        }

        function dictModeClicked()
        {
            if(devManager.getScanForWhat()===1)
            {
                devManager.setScanForWhat(0);
                devManager.saveConfigFile();
                res["mode"] = "translate"
                pingManager.addPingData(PingDefine.PING_EVENT_NAVI_SWITCH_MODE,res);
            }
            naviPanel.resetModel();
        }



        Rectangle {
            id: inner
            anchors.fill: parent
            color: Style.mainbgColor
            focus: false
            radius: Style.highVersion ? 15:0
            enabled: !wordView.visible

            Rectangle
            {
                id:drawerTip
                visible: true
                width: 120
                height: 8
                radius: 5
                color: "#D8D8D8"
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                opacity: 0.22
            }


            SetShortMenu
            {
                id:topdrawer
                x:0
                y: -Style.gHeight
                width: Style.gWidth
                height: Style.gHeight
                z:200
                //rotation: devManager.isLeftHand ?180:0
                Behavior on y {PropertyAnimation {duration: 130} }

                onSig_open:
                {
                    y = 0
                    //visible = true
                }

                onSig_close:
                {
                    y = -Style.gHeight
                    //visible = false
                }

                //PropertyAnimation on y {to: 50; duration: 1000; loops: Animation.Infinite}
            }
            SwipeArea
            {
                id:drawerArea
                z:2
                anchors.left: deviceMode.right
                anchors.leftMargin: 30
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.right: parent.right
                anchors.rightMargin: 180
                drag.minimumY: -30
                height: 110

                onDownmove:{
                    topdrawer.open()
                }
            }

            MainSwitch{
                id:deviceMode
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.left: parent.left
                anchors.leftMargin: 20
                leftText: qsTr("查词")
                rightText: qsTr("答疑")
                z:10
                onCheckedChanged: {
                    if(checked){
                        main.dictModeClicked();
                    }
                    else{
                        main.tidModeClicked();
                    }
                }

            }


            Image {
                id:bluetooth
                source: ""
                anchors.verticalCenter: elecVal.verticalCenter
                anchors.right: flow4g.left
                anchors.rightMargin: 6
            }

            Image {
                id:flow4g
                source: ""
                anchors.verticalCenter: elecVal.verticalCenter
                anchors.right: wifi.left
                anchors.rightMargin: 6
            }

            Image {
                id:wifi
                source: ""
                anchors.verticalCenter: elecVal.verticalCenter
                anchors.right: curDateTime.left
                anchors.rightMargin: 6
            }


            ZybText {
                id: curDateTime
                font.pixelSize: 24
                font.styleName: "Bold"
                text: Utils.currentDataTimeHHMMSS();
                anchors.verticalCenter: elecVal.verticalCenter
                anchors.right: elecText.left
                anchors.rightMargin: 18
                //定时器
                Timer{
                    id: timer
                    interval: 1 //间隔(单位毫秒):1000毫秒=1秒
                    repeat: true //重复
                    running: true
                    onTriggered:{
                        curDateTime.text = Utils.currentDataTimeHHMMSS();
                    }
                }
            }


            ZybText
            {
                id:elecText
                font.pixelSize: 24
                font.styleName: "Bold"
                color:"#FFFFFF"
                anchors.verticalCenter: elecVal.verticalCenter
                anchors.right: elecVal.left
                anchors.rightMargin: 4
            }

            Bettery {
                id:elecVal
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 18
            }

            NaviPanel {
                id: naviPanel
                anchors.fill: parent
                onSceneSourceChanged: {
                    console.log("onSceneSourceChanged:",sceneSource)
                    if(sceneSource === "qrc:/ZybQmlFiles/ZybLayoutViews/ZybScanWordView2.qml"){
                        sceneLoader.source = ""
                        wordView.showType = "scan"
                        wordView.visible = true;
                        //wordView.pageState = 0;
                        wordView.goToTipPage();
                        topdrawer.interactive = false;
                    }
                    else{
                        if(sceneSource == "qrc:/ZybQmlFiles/ZybPhone/Phone.qml")
                        {
                            if(dataManager.getLive() === false)
                            {
                                naviPanel.sceneSource = ""
                                topdrawer.interactive = true;
                                inner.visible = true
                                netDig.open();
                                return;
                            }
                        }

                        //                        if(devManager.getHaveShowCameraHelp() === false){
                        //                            if(sceneSource == "qrc:/ZybQmlFiles/ZybCamera/CameraPage.qml")
                        //                            {
                        //                                naviPanel.sceneSource = ""
                        //                                topdrawer.close();
                        //                                wifiUtils.close();
                        //                                dlgUtils.close();
                        //                                UILogic.openTipPage("cameraquestion");
                        //                                devManager.setHaveShowCameraHelp();
                        //                                return;
                        //                            }
                        //                        }

                        sceneLoader.source = sceneSource
                        var scene = null
                        var innerVisible = true
                        if (sceneSource == "") {
                            inner.visible = true
                            //naviPanel.updateSetIcon();
                        } else {
                            topdrawer.interactive = false;
                            inner.visible = false

                            scene = sceneLoader.item
                            if (scene) {
                                main.bEnterPicFormKey  =false;
                                main.bEnterSpeechFormKey = false
                                main.bEnterCameraFormKey = false

                                scene.parent = main
                                scene.anchors.fill = main
                            }


                        }
                    }

                }
            }
        }


        function close() {
            if(wordView.visible){
                wordView.visible = false;
            }
            naviPanel.sceneSource = ""
            topdrawer.interactive = true;
        }

        function backToSetPage() {
            close();
            console.log("backToSetPage");
            naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybSet/SetMainPage.qml"
        }

        function showWordResView(strInput){
            if(strInput.length <= 0){
                return;
            }
            wordView.showType = "history"
            wordView.goToWordResultPage(strInput,true);
            wordView.visible = true;
        }

        function closeWordResView(){
            wordView.visible = false;
            topdrawer.interactive = true;
        }

        function wordResViewToPos(pos){
            wordView.resViewToPos(pos);
        }

        function getwordResViewPos(){
            return wordView.getResPos();
        }


        ZybKeyBoard
        {
            id:vk
            z:4000
            width: parent.width
            height: parent.height
            x:0
            y : Style.gHeight
        }


        MsgTip
        {
            id:msg
            visible: false
            Timer{
                id:tiptimer
                interval: 2000;
                repeat: false;
                running: false
                triggeredOnStart: false
                onTriggered: {
                    msg.y = Style.gHeight
                    msg.visible = false
                }
            }

        }

        function showTip(str,showTime){
            var interTime;
            if(!showTime || !UILogic.isValid(showTime)){
                interTime = 2000;
            }
            else{
                interTime = showTime;
            }
            msg.visible = true
            msg.content = str
            if(msg.y == Style.gHeight)
            {
                msg.y -= msg.height
            }
            else
            {
                msg.y = Style.gHeight
            }
            tiptimer.interval = interTime;
            tiptimer.restart();
        }

        function closeTip(){
            msg.y = Style.gHeight
            msg.visible = false;
            tiptimer.stop();
        }


        RotationImage
        {
            id:busyImg
            source: "qrc:/images/setLoading46x44.png"
            anchors.centerIn: parent
            visible: false
            z:999
            running: visible===true?true:false
        }

        PopupPage {
            id: messageDialog
            x:devManager.isLeftHand?(parent.width - (parent.width - width)/2):((parent.width - width) / 2)
            y:devManager.isLeftHand?(parent.height-(parent.height - height) / 2):((parent.height - height) / 2)
            width: Style.gWidth
            height: Style.gHeight

            Timer{
                id:powofftimer
                interval: 2000;
                repeat: false;
                running: false
                onTriggered: {
                    engineSocket.sendPoweroff();
                }
            }

            onOpened:
            {
                sslClient.setKeep(false);
                sslClient.closeSocket();
                //dataManager.clearAgoraLog();
                vk.hideKeyboard();
                powofftimer.start();
            }
        }


        PopupDialog
        {
            id:netDig
            x:devManager.isLeftHand?(parent.width - (parent.width - width)/2):((parent.width - width) / 2)
            y:devManager.isLeftHand?(parent.height-(parent.height - height) / 2):((parent.height - height) / 2)
            closeBtnVisible:true
            leftTxtVisble:false
            content:qsTr("网络未连接，请先连接网络")
            rightTxt:qsTr("去设置")

            onRightClicked:
            {
                if(wordView.visible){
                    wordView.visible = false;
                }
                netDig.close();
                dlgUtils.close();
                naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybSet/SetMainPage.qml";
            }
        }


        PopupTip
        {
            id:globalTip
            x:devManager.isLeftHand?(parent.width - (parent.width - width)/2):((parent.width - width) / 2)
            y:devManager.isLeftHand?(parent.height-(parent.height - height) / 2):((parent.height - height) / 2)
            visible: false
        }

        LowPowerPhoneOnlineDig
        {
            id:lowpowerPhoneWarnDig
            x:devManager.isLeftHand?(parent.width - (parent.width - width)/2):((parent.width - width) / 2)
            y:devManager.isLeftHand?(parent.height-(parent.height - height) / 2):((parent.height - height) / 2)
            visible: false
        }

        function setWifiIcon(signum)
        {
            switch(signum)
            {
            case 0:
                wifi.source = "qrc:/images/statusbar_icon_wifi_0.png"
                break;
            case 1:
                wifi.source = "qrc:/images/statusbar_icon_wifi_1.png"
                break;
            case 2:
                wifi.source = "qrc:/images/statusbar_icon_wifi_2.png"
                break;
            case 3:
                wifi.source = "qrc:/images/statusbar_icon_wifi_2.png"
                break;
            case 4:
                wifi.source = "qrc:/images/statusbar_icon_wifi_3.png"
                break;
            }
        }

        function setWifiIconWhenOffline(signum)
        {
            switch(signum)
            {
            case 0:
                wifi.source = "qrc:/images/statusbar_icon_wifi_0_error.png"
                break;
            case 1:
                wifi.source = "qrc:/images/statusbar_icon_wifi_1_error.png"
                break;
            case 2:
                wifi.source = "qrc:/images/statusbar_icon_wifi_2_error.png"
                break;
            case 3:
                wifi.source = "qrc:/images/statusbar_icon_wifi_2_error.png"
                break;
            case 4:
                wifi.source = "qrc:/images/statusbar_icon_wifi_3_error.png"
                break;
            }
        }

        function updateTidAndDictMode()
        {
            console.log("updateTidAndDictMode",devManager.getScanForWhat());
            //            tidMode.title = devManager.getNetMainTitle();
            deviceMode.rightText = devManager.getNetMainTitle();
            if(devManager.getScanForWhat()===0)
            {
                //                tidMode.checked = false;
                //                dictMode.checked = true;
                deviceMode.checked = true
            }
            else
            {
                //                tidMode.checked = true;
                //                dictMode.checked = false;
                deviceMode.checked = false
            }

        }

        Connections
        {
            target: apiNetWork
            onMainTitleChanged:
            {
                naviPanel.updateTidTitle();
                main.updateTidAndDictMode();
            }


            onSig_getDevInfoSuccess:
            {
                naviPanel.updateSetIcon();
            }

        }

        Connections {
            target: scanManager;

            onSig_scanDown:{
                if(main.bReady===false)
                    return;

                topdrawer.close();
                wifiUtils.close();
                dlgUtils.close();
                if (naviPanel.sceneSource === "") {
                    if(devManager.getScanForWhat() === 0)
                    {
                        netDig.close();
                        naviPanel.sceneSource = "";
                        sceneLoader.source = ""
                        topdrawer.interactive = false;
                        if(wordView.visible === false){
                            wordView.showType = "scan"
                            wordView.visible = true;
                            wordView.goToScanPage();

                        }
                        else{
                            wordView.goToScanPage();
                        }

                    }
                    else if(devManager.getScanForWhat() === 1)
                    {
                        main.bEnterPicFormKey = true;
                        naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybScanForTid/ScanPicPage.qml";
                    }
                } else {
                    var scene = sceneLoader.item;

                    if(devManager.getScanForWhat() === 0)
                    {
                        netDig.close();
                        if(naviPanel.sceneSource !== "qrc:/ZybQmlFiles/ZybLayoutViews/ZybScanWordView2.qml"){
                            naviPanel.sceneSource = "";
                            sceneLoader.source = ""
                            topdrawer.interactive = false;
                        }

                        if(wordView.visible === false){
                            wordView.showType = "scan"
                            wordView.visible = true;
                            wordView.goToScanPage();

                        }
                        else{
                            wordView.goToScanPage();
                        }
                    }
                    else if(devManager.getScanForWhat() === 1)
                    {
                        if(scene.pageType !== "picSearchPage"){
                            UILogic.backToHome();
                            main.bEnterPicFormKey = true
                            naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybScanForTid/ScanPicPage.qml";
                        }
                        else{
                            scene.pageState = 1;
                        }
                    }


                }
            }
        }

        Connections {
            target: dataManager;
            onSig_wifiNetOnline:{
                if(wifiManager.isConnected())
                    var signalmun = wifiManager.getLastConnectedSignalNum();
                console.log("onSig_wifiIsNet",status,signalmun);
                if(status === true)
                {
                    main.setWifiIcon(signalmun);
                }
                else
                {
                    main.setWifiIconWhenOffline(signalmun);
                }
                if(GlobalProperty.isOtaDownloadAbord){
                    GlobalProperty.isOtaDownloadAbord = false;
                    OtaManager.downLoad(devManager.getNewUrl());
                    devManager.setIsDownloading(true);
                }
            }

            onSig_netLive:
            {
                if(sta)
                    netDig.close();
            }

        }

        Connections {
            target: engineSocket;

            onSig_netIsOff:
            {
                if(main.bReady===false)
                    return;
                topdrawer.close();
                netDig.open();
            }

            onSig_betteryClose:{
                elecVal.setNormal();
            }

            onSig_betteryOpen:{

                elecVal.setCharging();
            }

            onSig_betteryPer:{
                elecVal.setPer(num)
                elecText.text = num+"%"
                //                console.log("onSig_betteryPer:",num);

                if(main.bReady)
                {
                    if(num === 20 && devManager.getPowerChargeing()===false)
                    {
                        if(main.bWarnLow20Elec === false)
                        {
                            //if(devManager.getIsPhoneOnline())
                            if(sslClient.getKeep())
                            {
                                lowpowerPhoneWarnDig.open();
                            }
                            else
                            {
                                globalTip.content = qsTr("设备电量低于20%")
                                globalTip.open();
                            }
                            main.bWarnLow20Elec = true;
                        }
                    }

                    if(num === 10 && devManager.getPowerChargeing()===false)
                    {
                        if(main.bWarnLow10Elec === false)
                        {
                            globalTip.content = qsTr("设备电量低于10%")
                            globalTip.open();
                            main.bWarnLow10Elec = true;
                        }
                    }

                    //                    console.log("getPowerChargeing:",devManager.getPowerChargeing(),"getNeedShowUpdateBettery:",devManager.getNeedShowUpdateBettery());
                    if(num >= 70 && devManager.getPowerChargeing()===true)
                    {
                        if(devManager.getNewVersionForced() && devManager.getNewVerDownload()){
                            engineSocket.sendPhoneComeingScreenOn();
                            forceOtaTimer.restart();
                            dlgUtils.openForceOtaDlg(qsTr("重要更新，请立即升级"),qsTr(" "),qsTr("立即升级"),
                                                     function() {
                                                         console.log("force engineSocket.sendUpdate()++++++++++++++++");
                                                         devManager.setNewVersionForced(false);
                                                         devManager.saveConfigFile();
                                                         engineSocket.sendUpdate();
                                                     },
                                                     function() {
                                                     },
                                                     false,false);
                            devManager.setNeedShowUpdateBettery(false);
                            return;
                        }
                        if(bForecOtaStart)
                            return;
                        if(devManager.getNeedShowUpdateBettery() === true  || devManager.getNeedShowUpdateBettery())
                        {
                            UILogic.confirmOperator(qsTr("设备电量已经高于70%，是否立即安装更新"),qsTr("否"),qsTr("是"),
                                                    function() {
                                                        if(devManager.getPowerNum() < 70)
                                                        {
                                                            //                                                    main.showTip(qsTr("请将电量充至70%以上再升级"));
                                                            UILogic.confirmOperator(qsTr("请将电量充至70%以上再进行安装"),qsTr(" "),qsTr("知道了"),
                                                                                    function() {
                                                                                    },
                                                                                    function() {
                                                                                    },
                                                                                    false,false);
                                                            devManager.setNeedShowUpdateBettery(true);
                                                            return;
                                                        }
                                                        engineSocket.sendUpdate();
                                                    },
                                                    function() {
                                                    },
                                                    false,true);
                            devManager.setNeedShowUpdateBettery(false);
                        }
                    }
                }


                if(num > 15)
                {
                    main.bWarnLow10Elec = false
                }

                if(num > 25)
                {
                    main.bWarnLow20Elec = false
                }
            }

            onSig_home:
            {

                main.close()
                topdrawer.close();
                busyImg.visible = false
                msg.y = Style.gHeight
                vk.hideKeyboard();
                wifiUtils.close();
                dlgUtils.close();
            }

            onSig_shutdown:{
                devManager.saveConfigFile();
                dataManager.truncateCache();
                messageDialog.open()
                console.log("qt.quit");
            }

            onSig_wifiStatusChanged:{
                console.log("onSig_wifiStatusChanged:",sta)
                switch(sta)
                {
                case 0:
                    wifi.source = ""
                    break;
                case 1:
                    wifi.source = ""
                    break;
                case 2:
                    main.setWifiIcon(signum);
                    flow4g.source = "";
                    break;
                }

            }

            onSig_wifiSignalChanged:
            {
                flow4g.source = "";
                main.setWifiIcon(signum);
            }

            onSig_btOn:
            {
                if(res)
                {
                    bluetooth.source = "qrc:/images/main_bt.png"
                }
                else
                {
                    bluetooth.source = ""
                }
            }

            onSig_btOff:
            {
                if(res)
                {
                    bluetooth.source = ""
                }
            }
            onSig_btAutoConnected:
            {
                bluetooth.source = "qrc:/images/main_blzheadset.png"
            }

            onSig_btConnected:
            {
                console.log("btconnected................");
                bluetooth.source = "qrc:/images/main_blzheadset.png"
            }

            onSig_btStatusChanged:
            {
                if(sta === 1)
                {
                    bluetooth.source = "qrc:/images/main_bt.png"
                }
                else if(sta === 0)
                {
                    bluetooth.source = ""
                }
            }

            onSig_btDisconnect:
            {
                bluetooth.source = "qrc:/images/main_bt.png"
            }

            onSig_4gOn:
            {
                if(mode === 1)
                {
                    if(dataManager.getLive())
                    {
                        flow4g.source = "";
                        return;
                    }

                    switch(level)
                    {
                    case 0:
                        flow4g.source = "qrc:/images/flow0_onlyOn.png";
                        break;
                    case 1:
                        flow4g.source = "qrc:/images/flow1_onlyOn.png";
                        break;
                    case 2:
                        flow4g.source = "qrc:/images/flow2_onlyOn.png";
                        break;
                    case 3:
                        flow4g.source = "qrc:/images/flow3_onlyOn.png";
                        break;
                    case 4:
                        flow4g.source = "qrc:/images/flow4_onlyOn.png";
                        break;
                    }
                }
                else if(mode === 2)
                {
                    switch(level)
                    {
                    case 0:
                        flow4g.source = "qrc:/images/flow0.png";
                        break;
                    case 1:
                        flow4g.source = "qrc:/images/flow1.png";
                        break;
                    case 2:
                        flow4g.source = "qrc:/images/flow2.png";
                        break;
                    case 3:
                        flow4g.source = "qrc:/images/flow3.png";
                        break;
                    case 4:
                        flow4g.source = "qrc:/images/flow4.png";
                        break;
                    }
                }

            }

            onSig_4gOff:
            {
                flow4g.source = "";
            }

            onSig_recordStart:
            {
                if(main.bReady===false)
                    return;
                console.log("main.qml onSig_recordStart");
                topdrawer.close();
                globalTip.close();
                lowpowerPhoneWarnDig.close();
                dlgUtils.close();
                vk.hideKeyboard();

                if (naviPanel.sceneSource == "") {
                    main.bEnterSpeechFormKey = true
                    naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybVoiceTrans/VoiceTrans.qml";
                    UILogic.closeWordResView();
                } else {
                    if(wordView.visible){
                        main.bEnterSpeechFormKey = true
                        naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybVoiceTrans/VoiceTrans.qml";
                        UILogic.closeWordResView();
                    }
                    else{
                        var scene = sceneLoader.item;
                        if(scene.pageType !== "voicetranspage"){
                            main.bEnterSpeechFormKey = true
                            naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybVoiceTrans/VoiceTrans.qml";
                        }

                    }

                }
            }

            onSig_cameraKey:
            {
                if(main.bReady===false)
                    return;
                topdrawer.close();
                wifiUtils.close();
                dlgUtils.close();

                //                if(devManager.getHaveShowCameraHelp() === false){
                //                    UILogic.openTipPage("cameraquestion");
                //                    devManager.setHaveShowCameraHelp();
                //                    return;
                //                }

                if (naviPanel.sceneSource == "") {
                    main.bEnterCameraFormKey = true
                    pingManager.addPingData(PingDefine.PING_EVENT_TID_KEY_CAMERA_NUM);
                    naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybCamera/CameraPage.qml";
                    UILogic.closeWordResView();
                } else {
                    if(wordView.visible){
                        wordView.visible = false;
                        main.bEnterCameraFormKey = true
                        naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybCamera/CameraPage.qml";
                        UILogic.closeWordResView();
                    }
                    else{
                        var scene = sceneLoader.item;
                        if(scene.pageType !== "camerapage"){
                            main.bEnterCameraFormKey = true
                            naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybCamera/CameraPage.qml";
                        }
                    }

                }
            }
        }
        Connections{
            target: sslClient
            enabled : naviPanel.sceneSource !== "qrc:/ZybQmlFiles/ZybPhone/Phone.qml"
            onCallComeing:
            {
                if(main.bReady===false)
                    return;
                console.log("main.qml onCallComeing");
                topdrawer.close();
                globalTip.close();
                dlgUtils.close();
                lowpowerPhoneWarnDig.close();

                if (naviPanel.sceneSource == "") {
                    main.bEnterPhoneFormOut = true;
                    naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybPhone/Phone.qml";
                    UILogic.closeWordResView();
                } else {
                    if(wordView.visible){
                        main.bEnterPhoneFormOut = true;
                        naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybPhone/Phone.qml";
                        UILogic.closeWordResView();
                    }
                    else{
                        var scene = sceneLoader.item;
                        if(scene.pageType !== "phone"){
                            main.bEnterPhoneFormOut = true;
                            naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybPhone/Phone.qml";
                        }
                        else{
                            scene.state = "incoming";
                        }

                    }

                }
            }
        }
    }

    property string mmdialogStatus: ""
    Connections{
        target: mmPrintManager
        onSig_mmLowPower:{
            if(mmdialogStatus !== "LowPower"){
                UILogic.confirmOperator(qsTr("喵喵机电量过低"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "LowPower"
            }

//            UILogic.showTip(qsTr("喵喵机快没电啦"))
        }

        onSig_mmNoPower:{
            if(mmdialogStatus !== "NoPower"){
                UILogic.confirmOperator(qsTr("打印中断，喵喵机没电啦"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "NoPower"
            }

//            UILogic.showTip(qsTr("喵喵机没电啦"))
        }

        onSig_mmNoPaper:{
            if(mmdialogStatus !== "NoPaper"){
                UILogic.confirmOperator(qsTr("打印暂停，喵喵机没纸啦"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "NoPaper"
            }

//            UILogic.showTip(qsTr("喵喵机没纸啦"))
        }

        onSig_mmOverHeat:{
            if(mmdialogStatus !== "OverHeat"){
                UILogic.confirmOperator(qsTr("打印暂停，喵喵机过热啦"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "OverHeat"
            }

//            UILogic.showTip(qsTr("喵喵机过热啦"))
        }

        onSig_mmLidOpen:{
            if(mmdialogStatus !== "LidOpen"){
                UILogic.confirmOperator(qsTr("打印暂停，喵喵机张嘴啦"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "LidOpen"
            }

//            UILogic.showTip(qsTr("喵喵机张嘴啦"))
        }

        onSig_mmIsPrinting:{
            if(mmdialogStatus !== "IsPrinting"){
                UILogic.confirmOperator(qsTr("正在打印中，请稍后重试"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "IsPrinting"
            }

//            UILogic.showTip(qsTr("喵喵机正在打印"))
        }

        onSig_mmAuthFailed:{
            if(mmdialogStatus !== "AuthFailed"){
                UILogic.confirmOperator(qsTr("设备授权失败，请连网授权"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "AuthFailed"
            }

//            UILogic.showTip(qsTr("喵喵机正在打印"))
        }

        onSig_mmNotSupport:{
            if(mmdialogStatus !== "NotSupport"){
                UILogic.confirmOperator(qsTr("暂不支持该型号设备"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "NotSupport"
            }
//            UILogic.showTip(qsTr("喵喵机正在打印"))
        }
        onSig_tipDeviceDisConnect:{
            if(mmdialogStatus !== "tipDeviceDisConnect"){
                UILogic.confirmOperator(qsTr("喵喵机断开啦"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        function() {
                                            mmdialogStatus = "";
                                        },
                                        false,false);
                mmdialogStatus = "tipDeviceDisConnect"
            }
        }
    }

    Timer{
        id:otaResTimer
        interval: 10;
        repeat: false;
        running: false
        triggeredOnStart: false
        onTriggered: {
            console.log("devManager.getUpdateStatus:",devManager.getUpdateStatus())
            var res = new Object;
            if(devManager.getUpdateStatus() === 3){
                OtaManager.otaFinished();
                res["res"] = "successed";
                pingManager.addPingData(PingDefine.PING_EVENT_OTA_RES,res);
                UILogic.confirmOperator(qsTr("升级成功"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            OtaManager.otaFinished();
                                        },
                                        function() {
                                        },
                                        false,false);
            }
            else if(devManager.getUpdateStatus() === 4){
                OtaManager.otaFinished();
                res["res"] = "failed";
                pingManager.addPingData(PingDefine.PING_EVENT_OTA_RES,res);
                UILogic.confirmOperator(qsTr("升级失败"),qsTr("取消"),qsTr("知道了"),
                                        function() {
                                            OtaManager.otaFinished();
                                        },
                                        function() {
                                        },
                                        false,false);
            }
        }
    }

    Timer
    {
        id :initTimer

        interval: 100; running: false; repeat: false
        onTriggered:
        {
            main.bReady = true;
            if(btManager.getStatus() === BtConnectStatus.Bt_Connected)
            {
                bluetooth.source = "qrc:/images/main_blzheadset.png"
            }
            else
            {
                if(btManager.getStatus() !== BtConnectStatus.Bt_Close)
                {
                    bluetooth.source = "qrc:/images/main_bt.png"
                }
            }

            if(devManager.getHalted()){
                devStateMonitor.setDevState(DevStatus.DevState_Locked);
                dlgUtils.openLockTipDlg();
            }
        }

    }

    property bool bForecOtaStart: false
    Timer{
        id:forceOtaTimer
        interval: 10000
        repeat: false
        running: false
        triggeredOnStart: false
        onTriggered: {
            console.log("force engineSocket.sendUpdate()++++++++++++++++");
            bForecOtaStart = true;
            devManager.setNewVersionForced(false);
            devManager.saveConfigFile();
            engineSocket.sendUpdate();
        }
    }


    Component.onCompleted:
    {
        main.updateTidAndDictMode();
        naviPanel.resetModel();
        if(devManager.getInitStatus() < 2){
            console.log("load qrc:/ZybQmlFiles/ZybInit/InitView.qml")
            naviPanel.sceneSource = "qrc:/ZybQmlFiles/ZybInit/InitView.qml"
        }
        else{

            otaResTimer.running = true;
        }


        initTimer.start();
        console.info("main.qml onCompleted")
        // The device reports the real level through onSig_betteryPer; the
        // getter can still return 0 right after a restart, so only seed the
        // indicator when a positive value is available.
        var power = devManager.getPowerNum();
        if (typeof power === "number" && power > 0) {
            elecVal.setPer(power);
            elecText.text = power + "%";
        }
    }



    Connections{
        target: devManager
        onOtaCheckFinshed:{
            console.log("isLastVersion:",devManager.getLatested(),"newverdlgver:",devManager.getNewVerDlgVer(),"newver:",devManager.getNewVersion())
            if(GlobalProperty.isUpdatePageVisible || devManager.getNewVersionForced()){
                if(devManager.getNewVerDlgVer() !== devManager.getNewVersion()){
                    devManager.setUpdateRedEntered(false);
                    devManager.setNewVerDlgVer(devManager.getNewVersion());
                }
                return;
            }

            if(devManager.getInitStatus() < 2){
                return;
            }

            var isLastVersion = devManager.getLatested();
            if(isLastVersion !== true || !isLastVersion){
                if(devManager.getNewVerDlgVer() !== devManager.getNewVersion()){
                    UILogic.confirmOperator(qsTr("检查到新版本"),qsTr("取消"),qsTr("立即更新"),function(){
                        UILogic.startOta();
                    },
                    function(){},
                    true,true)
                    devManager.setUpdateRedEntered(false);
                }
                devManager.setNewVerDlgVer(devManager.getNewVersion());
            }

        }

        onSig_haltedChanged:{
            if(halted){
                devStateMonitor.setDevState(DevStatus.DevState_Locked);
                dlgUtils.openLockTipDlg();
            }
            else{
                if(devStateMonitor.getDevState() === DevStatus.DevState_Locked)
                {
                    devStateMonitor.setDevState(DevStatus.DevState_Normal);
                }
                dlgUtils.closeLockDlg();
            }
        }
    }

    Connections{
        target: OtaManager
        onSig_downLoadError: {
            //            devManager.removeOtaPackge();
            if(devManager.getNewVersionForced()){
                OtaManager.downloadFailed();
                return;
            }
            UILogic.confirmOperator(qsTr("下载升级包失败"),qsTr("取消"),qsTr("知道了"),
                                    function() {
                                        OtaManager.downloadFailed();
                                        //                                        devManager.setReceiveUpdateSig(false);
                                        //                                        devManager.setIsUpdateSuccess(false);
                                        //                                        devManager.setIsDownloading(false);
                                        //                                        devManager.removeOtaPackge();
                                        //                                        devManager.setNewVerDownload(false);
                                        console.log("Ok clicked");
                                    },
                                    function() {
                                    },
                                    false,false);
        }
        onSig_4gDownload:{
            console.log("onSig_4gDownload +++++++++++++")
            if(devManager.getNewVersionForced()){
                OtaManager.downLoad(devManager.getNewUrl());
                devManager.setIsDownloading(true);
                return;
            }
            UILogic.confirmOperator(qsTr("正在使用流量,是否连接WiFi下载"),qsTr("继续下载"),qsTr("连接WiFi"),
                                    function() {
                                        GlobalProperty.isOtaDownloadAbord = true;
                                        if(!GlobalProperty.isWifiPageVisible){
                                            UILogic.openWifiPage(function(){
                                                //                                            OtaManager.downLoad(devManager.getNewUrl());
                                                //                                            devManager.setIsDownloading(true);
                                            },
                                            function(){
                                                //                                            OtaManager.stopDownload();
                                                //                                            devManager.setIsDownloading(false);
                                                //                                            OtaManager.cancelDownload();
                                            })
                                        }

                                    },
                                    function() {
                                        OtaManager.downLoad(devManager.getNewUrl());
                                        devManager.setIsDownloading(true);
                                    },
                                    false,true);
        }
        onDownLoadFinished:{
            if(devManager.checkOtaPackge()){
                if(devManager.getNewVersionForced()){
                    if(devManager.getPowerNum() < 70)
                        return;
                    engineSocket.sendPhoneComeingScreenOn();
                    forceOtaTimer.restart();
                    dlgUtils.openForceOtaDlg(qsTr("重要更新，请立即升级"),qsTr(" "),qsTr("立即升级"),
                                             function() {
                                                 console.log("force engineSocket.sendUpdate()++++++++++++++++");
                                                 devManager.setNewVersionForced(false);
                                                 devManager.saveConfigFile();
                                                 engineSocket.sendUpdate();
                                             },
                                             function() {
                                             },
                                             false,false);

                    return;
                }
                if(!GlobalProperty.isUpdatePageVisible){
                    UILogic.confirmOperator(qsTr("更新下载完成，是否立即安装"),qsTr("取消"),qsTr("确认"),
                                            function() {
                                                if(devManager.getPowerNum() < 70)
                                                {
                                                    //                                                    main.showTip(qsTr("请将电量充至70%以上再升级"));
                                                    UILogic.confirmOperator(qsTr("请将电量充至70%以上再进行安装"),qsTr(" "),qsTr("知道了"),
                                                                            function() {
                                                                            },
                                                                            function() {
                                                                            },
                                                                            false,false);
                                                    devManager.setNeedShowUpdateBettery(true);
                                                    return;
                                                }
                                                engineSocket.sendUpdate();
                                            },
                                            function() {
                                                console.log("cancel clicked");
                                            },
                                            false,true);
                }

            }
            else{
                if(devManager.getNewVersionForced()){
                    return;
                }
                UILogic.confirmOperator(qsTr("下载升级包失败"),qsTr("取消"),qsTr("知道了"),
                                        function() {

                                            console.log("Ok clicked");
                                        },
                                        function() {
                                        },
                                        false,false);
            }
        }
    }
}

