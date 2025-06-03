
const views = ["Digital Clock", "Analog Clock", "Position", "Client IPs", "UpTime", "Wifi Quality"];

function pageLoad() {
    closeMenu();
    showTimeSettings();
    document.getElementById('content').className = 'content'; //??
}
// Clients
function showClients() {
    showView("Clients");
    LoadClients();
}

function LoadClients() {
    sendRequest("clients.dat", respose_clients);
}

function respose_clients(data) {
    // List of ip-clients 
    var element = document.getElementById("ClientsList");
    element.value = "";
    const obj = JSON.parse(data);
    for (var i = 0; i < obj.length; i++) {
        element.value += " IP: " + obj[i].IP + " Time: " + obj[i].last + " Syncs: " + obj[i].syncs + "\n";
    }
}
//unused function UpdateClients(){} 

// show and hide the notification bar
function openNotification(msg, timeout = 0) {
    // document.getElementById("notification").innerHTML = msg;

    var elem = document.getElementById("notification");
    var content = document.getElementById("content");
    elem.classList.add("open");
    if (msg)
        alert(msg);
    //content.innerHTML = msg;

    elem.style.top = content.offsetHeight / 2 - 70 + 60 + "px";
    var left = content.offsetWidth / 2;
    var mleft = content.offsetLeft;
    left = (left + mleft) - 80;
    /*  - 60
    if (left > 600)
        left = left / 2 + 125 + "px";
    else
        left = (left) / 2  - 200 + 125 + "px";
    */
    elem.style.left = left + "px";

    if (timeout != 0)
        setTimeout(closeNotification, timeout);
}

function closeNotification() {
    document.getElementById("notification").classList.remove("open");
}


function showMQTTPage() {
    sendRequest("mqtt/settings", read_mqttsettings);
    showView("MQTTView");
}

//get and set wifi settings
function showLocationSettings() {
    //openNotification("Getting data...", 0);
    sendRequest("gps/data", getLocationSettingsHandler);
    showView("LocationSettings");
}

function getLocationSettingsHandler(data) {
    const obj = JSON.parse(data);
    var element = document.getElementById("latitude");
    element.value = obj.lat + " ° ";
    element = document.getElementById("longitude");
    element.value = obj.lon + " ° ";
    element = document.getElementById("altitude");
    element.value = obj.alti + " m ";
    element = document.getElementById("hdop");
    element.value = obj.hdop + " %";

    if (obj.valid)
        document.getElementById("posValid").innerHTML = "Position is valid!";
    else
        document.getElementById("posValid").innerHTML = "Position not valid!";

}

//WIFI
function setWifiUI() {
    document.getElementById("ssid").innerHTML = "<option value='0'>Scanning networks ...</option>";
    openNotification();
}

function getSSIDList() {
    setWifiUI();
    sendRequest("getSSIDList", getSSIDListHandler);
}

function getWiFiSettings() {
    setWifiUI();
    sendRequest("getWiFiSettings", getWiFiSettingsHandler);
}

function getSSIDListHandler(data) {
    closeNotification();
    var list = document.getElementById("ssid");
    var jsonObj = JSON.parse(data)
    if (data == null || data.length == 0) {
        list.innerHTML = "<option value='0'>no network found!</option>";
        return;
    }
    else
        list.innerHTML = "<option value='0'>found " + jsonObj.data.length + " Networks</option>";

    for (var i = 0; i < jsonObj.data.length; i++) {
        list.innerHTML += "<option value='" + jsonObj.data[i] + "'>" + jsonObj.data[i] + "</option>";
    }
}

function getWiFiSettingsHandler(data) {
    var jsonObj = JSON.parse(data)
    document.getElementById("hostname").value = ((jsonObj.host != "") ? jsonObj.host : "");
    document.getElementById("currentSSID").value = ((jsonObj.ssid != "") ? jsonObj.ssid : "No network configured");
    getSSIDListHandler(data);
}

//get and set wifi settings
function setWiFiSettings() {
    var ssid = document.getElementById("ssid").value;
    var pass = document.getElementById("pass").value;
    console.log("ssid: " + ssid + ", pass: " + pass);
    var request = "setWiFiSettings?ssid=" + ssid + "&pass=" + pass + "&clear=false";
    sendRequest(request, openNotification);
}

function setHostname() {
    var request = "setHostname?hostname=" + document.getElementById("hostname").value;
    console.log("request: " + request);
    sendRequest(request, openNotification);//, openNotification
}

//show the wifi settings data
function showWiFiSettings() {
    showView("WiFiSettings");
    getWiFiSettings();
}

function clearWiFiSettings() {
    var ssid = "";
    var pass = "";
    console.log("Clear WiFi Settings");
    var request = "setWiFiSettings?ssid=" + ssid + "&pass=" + pass + "&clear=true";
    sendRequest(request, openNotification);
}

// Time & Date
function showTimeSettings() {
    sendRequest("timesettings", read_timesettings);
    showView("TimeSettings");
}

function read_timesettings(msg) {

    var jsonObj = JSON.parse(msg)
    /*
            document.getElementById("ZONE_OVERRRIDE").checked = jsonObj.zoneoverride;
            document.getElementById("DLSOverrid").checked = jsonObj.dlsdis;
            document.getElementById("ManualDLS").checked = jsonObj.dlsmanena;
    
            var element = document.getElementById("GMT_OFFSET");
            element.value = jsonObj.gmtoffset;
    
            var element = document.getElementById("dls_offset");
            element.value = jsonObj.dlsmanidx;
    
            document.getElementById("GPS_ROLLLOVERCNT").value = jsonObj.gps_rollovercnt;
    */
    // We need also to read if we sync on GPS 

    var element = document.getElementById("timezoneid");
    element.value = jsonObj.tzidx;
    /*
            var date = new Date();
            var currentDate = date.toISOString().substring(0,10);
            var currentTime = date.toISOString().substring(11,16);
            
            document.getElementById('dateid').value  = "12.03.2023";//currentDate;
            document.getElementById('timeid').value  = "12:34:56";//currentTime;
     */
    var element = document.getElementById("dateid");
    element.value = jsonObj.date;

    var element = document.getElementById("timeid");
    element.value = jsonObj.time;

    var element = document.getElementById("timevalid");
    element.innerHTML = jsonObj.timevalid;

}

//IPv4
function showIPv4Settings() {
    sendRequest("ipv4settings.json", read_ipv4settings);
    showView("IPv4Settings");
}

//Notes
function showNotes() {
    showView("Notes");
    LoadNotes();
}

function UpdateNote() {

    var url = buildUrl("/notes.dat");
    var element = document.getElementById("NotesText");
    var text = element.value;
    text = text.substring(0, 500);

    var data = [];
    data.push({
        key: "notes",
        value: text
    });
    sendData(url, data);

}

function LoadNotes() {
    sendRequest("notes.dat", read_notes);
}

function read_notes(msg) {
    //Plain text 
    var element = document.getElementById("NotesText");
    element.value = msg;

}

// Display
function showSettingsPage() {
    sendRequest("display/settings", read_display_settings);
    showView("SettingsPage");
}


function read_display_settings(msg) {
    var jsonObj = JSON.parse(msg);
    document.getElementById("SWAP_DISP").checked = jsonObj.swap_display;
    document.getElementById("showNmea").checked = jsonObj.show_nmea;
    document.getElementById("disp_Info").value = views[jsonObj.display_info - 1];
    document.getElementById("hostname").value = ((jsonObj.host != "") ? jsonObj.host : "");
    document.getElementById("GPS_SYNC_ON").checked = jsonObj.gps_sync;
}

function testAlarm() {
    sendRequest("testAlarm", openNotification);
}

//open and close the main menu
function openMenu() {
    document.getElementById("menu").classList.add("open");
}

function closeMenu() {
    document.getElementById("menu").classList.remove("open");
}

//enable alarm cb
function enableAlarm() {
    var cb = document.getElementById("alarmEnabled").checked;
    document.getElementById("alarmLevelRow").style.display = (cb ? "" : "none");
}

function showLocation() {
    showView("LocationSettings");
    showLocationSettings();
}

function restart() {
    sendRequest("restart", openNotification);
}

function showView(view) {
    Array.from(document.getElementsByClassName("views")).forEach(function (v) { v.style.display = "none"; });
    document.getElementById(view).style.display = "";
    Array.from(document.getElementsByClassName("menuBtn")).forEach(function (b) { b.classList.remove("active") });
    document.getElementById(view + "Btn").classList.add("active");
    closeMenu();
}

function LoadParameter() {
    sendRequest("timesettings", read_timesettings);

}

function SubmitTimeZone() {
    var url = buildUrl("/timezone.dat");
    var element = document.getElementById("timezoneid");
    var data = [];
    data.push({
        key: "timezoneid",
        value: element.value
    });

    sendData(url, data);
}

function SubmitDateTime() {
    var url = buildUrl("/settime.dat");

    var element = document.getElementById("dateid");
    var d = element.value;


    var element = document.getElementById("timeid");

    var t = element.value;
    var parts = t.split(':');
    //This will result in ideal conditions in three parts
    if (parts.length < 3) {
        //We assume only hours and minutes here ( iphone style )
        t = parts[0] + ":" + parts[1] + ":00";
    }
    var data = [];
    data.push({
        key: "date",
        value: d
    });
    data.push({
        key: "time",
        value: t
    });

    sendData(url, data);
}

function SubmitGPSPosition() {
    alert("Not implemented!");
}

function SubmitGPSsync() {
    var url = buildUrl("/gps/syncclock.dat");

    var gpssync = document.getElementById("GPS_SYNC_ON").checked;

    var data = [];
    data.push({
        key: "GPS_SYNC_ON",
        value: gpssync
    });
    sendData(url, data);
}

function SubmitOverrides() {
    var url = buildUrl("/overrides.dat");

    var zoneoverride = document.getElementById("ZONE_OVERRRIDE").checked;
    var dls_override = document.getElementById("DLSOverrid").checked;
    var dlsmanena = document.getElementById("ManualDLS").checked;


    var element = document.getElementById("GMT_OFFSET");
    var gmtoffset = element.value;

    var element = document.getElementById("dls_offset");
    var dlsoffsetidx = element.value;




    var data = [];
    if (true === zoneoverride) {
        data.push({
            key: "ZONE_OVERRRIDE",
            value: zoneoverride
        });
    }

    if (true === dls_override) {
        data.push({
            key: "dlsdis",
            value: dls_override
        });
    }

    if (true === dlsmanena) {
        data.push({
            key: "dlsmanena",
            value: dlsmanena
        });
    }

    data.push({
        key: "dlsmanidx",
        value: dlsoffsetidx
    });

    data.push({
        key: "gmtoffset",
        value: gmtoffset
    });

    sendData(url, data);


}

function SendSleepMode() {
    var url = buildUrl("/led/activespan.dat");

    var element = document.getElementById("sh_start");
    var start = element.value;


    var element = document.getElementById("sh_end");
    var end = element.value;

    var nightmode = document.getElementById("Enable Nightmode").checked;


    var data = [];
    data.push({
        key: "silent_start",
        value: start
    });
    data.push({
        key: "silent_end",
        value: end
    });
    data.push({
        key: "enable",
        value: nightmode
    });

    sendData(url, data);
}

function read_ipv4settings(msg) {

    var jsonObj = JSON.parse(msg);

    document.getElementById("STATIC_IP").checked = jsonObj.USE_STATIC;
    document.getElementById("IP_0").value = jsonObj.IP[0];
    document.getElementById("IP_1").value = jsonObj.IP[1];
    document.getElementById("IP_2").value = jsonObj.IP[2];
    document.getElementById("IP_3").value = jsonObj.IP[3];

    document.getElementById("SUB_0").value = jsonObj.SUBNET[0];
    document.getElementById("SUB_1").value = jsonObj.SUBNET[1];
    document.getElementById("SUB_2").value = jsonObj.SUBNET[2];
    document.getElementById("SUB_3").value = jsonObj.SUBNET[3];

    document.getElementById("GW_0").value = jsonObj.GATEWAY[0];
    document.getElementById("GW_1").value = jsonObj.GATEWAY[1];
    document.getElementById("GW_2").value = jsonObj.GATEWAY[2];
    document.getElementById("GW_3").value = jsonObj.GATEWAY[3];

    document.getElementById("DNS0_0").value = jsonObj.DNS0[0];
    document.getElementById("DNS0_1").value = jsonObj.DNS0[1];
    document.getElementById("DNS0_2").value = jsonObj.DNS0[2];
    document.getElementById("DNS0_3").value = jsonObj.DNS0[3];

    document.getElementById("DNS1_0").value = jsonObj.DNS1[0];
    document.getElementById("DNS1_1").value = jsonObj.DNS1[1];
    document.getElementById("DNS1_2").value = jsonObj.DNS1[2];
    document.getElementById("DNS1_3").value = jsonObj.DNS1[3];


}

function SubmitIPv4Settings() {
    var url = buildUrl("/ipv4settings.json");

    var data = [];

    var use_static = document.getElementById("STATIC_IP").checked;

    var ip = [];

    ip.push(document.getElementById("IP_0").value);
    ip.push(document.getElementById("IP_1").value);
    ip.push(document.getElementById("IP_2").value);
    ip.push(document.getElementById("IP_3").value);

    var sub = [];

    sub.push(document.getElementById("SUB_0").value);
    sub.push(document.getElementById("SUB_1").value);
    sub.push(document.getElementById("SUB_2").value);
    sub.push(document.getElementById("SUB_3").value);

    var gw = [];
    gw.push(document.getElementById("GW_0").value);
    gw.push(document.getElementById("GW_1").value);
    gw.push(document.getElementById("GW_2").value);
    gw.push(document.getElementById("GW_3").value);

    var dns0 = [];

    dns0.push(document.getElementById("DNS0_0").value);
    dns0.push(document.getElementById("DNS0_1").value);
    dns0.push(document.getElementById("DNS0_2").value);
    dns0.push(document.getElementById("DNS0_3").value);

    var dns1 = [];

    dns1.push(document.getElementById("DNS1_0").value);
    dns1.push(document.getElementById("DNS1_1").value);
    dns1.push(document.getElementById("DNS1_2").value);
    dns1.push(document.getElementById("DNS1_3").value);

    var obj = {};
    obj["USE_STATIC"] = use_static;
    obj["IP"] = (ip);
    obj["SUBNET"] = (sub);
    obj["GATEWAY"] = (gw);
    obj["DNS0"] = (dns0);
    obj["DNS1"] = (dns1);


    data.push({
        key: "JSON",
        value: JSON.stringify(obj)
    });

    sendData(url, data);


}

function read_mqttsettings(msg) {
    var jsonObj = JSON.parse(msg);
    document.getElementById("MQTT_USER").value = jsonObj.mqttuser;
    document.getElementById("MQTT_HOST").value = jsonObj.mqtthost;
    document.getElementById("MQTT_SERVER").value = jsonObj.mqttserver;
    document.getElementById("MQTT_PORT").value = jsonObj.mqttport;
    document.getElementById("MQTT_PASS").value = jsonObj.mqttpass;
    document.getElementById("MQTT_TOPIC").value = jsonObj.mqtttopic;
    document.getElementById("MQTT_PASS").type = "password";
    document.getElementById("MQTT_ENA").checked = jsonObj.mqttena;
}

function SubmitDisplaysettings(dir) {
    var url = buildUrl("/display/settings");
    var swapped = document.getElementById("SWAP_DISP").checked;
    var showNmea = document.getElementById("showNmea").checked;

    var data = [];
    data.push({
        key: "SWAP_DISP",
        value: swapped
    });
    data.push({
        key: "display",
        value: dir
    });
    data.push({
        key: "showNmea",
        value: showNmea
    });
    sendData(url, data, showSettingsPage);
    //  httpGetAsync(theUrl, callback) 

}

function SubmitMQTT() {
    var url = buildUrl("/mqtt/settings");

    var mqtt_user = document.getElementById("MQTT_USER").value;
    var mqtt_host = document.getElementById("MQTT_HOST").value;
    var mqtt_server = document.getElementById("MQTT_SERVER").value;
    var mqtt_port = document.getElementById("MQTT_PORT").value;
    var mqtt_topic = document.getElementById("MQTT_TOPIC").value;
    var mqtt_ena = document.getElementById("MQTT_ENA").checked;


    var data = [];
    data.push({
        key: "MQTT_ENA",
        value: mqtt_ena
    });

    data.push({
        key: "MQTT_PORT",
        value: mqtt_port
    });


    data.push({
        key: "MQTT_USER",
        value: mqtt_user
    });

    data.push({
        key: "MQTT_HOST",
        value: mqtt_host
    });


    data.push({
        key: "MQTT_SERVER",
        value: mqtt_server
    });

    data.push({
        key: "MQTT_TOPIC",
        value: mqtt_topic
    });

    if ("password" != document.getElementById("MQTT_PASS").type) {
        var mqtt_pass = document.getElementById("MQTT_PASS").value;
        data.push({
            key: "MQTT_PASS",
            value: mqtt_pass
        });

        document.getElementById("MQTT_PASS").value = "********";
        document.getElementById("MQTT_PASS").type = "password";

    }

    sendData(url, data);




}

function mqtt_pass_onclick() {

    document.getElementById("MQTT_PASS").value = "";
    document.getElementById("MQTT_PASS").type == "text";

}

function sendRequest(addr, func = null) {
    console.log("requesting: " + addr);
    var xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            console.log("Request finished: " + this.status);
            if (func != null)
                func(this.responseText);
        } else
            console.log("on-rsc: " + this.status);
    }
    xhr.open("GET", addr, true);
    xhr.send();
}

function sendData(url, data, func = null) {
    var XHR = new XMLHttpRequest();
    var urlEncodedData = "";
    var urlEncodedDataPairs = [];
    var name;

    for (name in data) {
        urlEncodedDataPairs.push(encodeURIComponent(data[name].key) + '=' + encodeURIComponent(data[name].value));
    }

    urlEncodedData = urlEncodedDataPairs.join('&').replace(/%20/g, '+');

    XHR.addEventListener('load', function (event) {
        /*   
       alert('upload complete.');
        */
        if (func != null)
            func();
    });

    XHR.addEventListener('error', function (event) {
        alert('Oops! Something goes wrong.');
    });

    XHR.open('POST', url);
    XHR.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    XHR.send(urlEncodedData);

}

function leftPad(num, size) {
    var s = num + "";
    while (s.length < size) s = "0" + s;
    return s;
}

function buildUrl(url) {
    var protocol = location.protocol;
    var slashes = protocol.concat("//");
    var host = slashes.concat(window.location.hostname);
    return host + url;
}


