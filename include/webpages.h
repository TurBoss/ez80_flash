
#ifndef WEBPAGES_H
#define WEBPAGES_H


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    body {
      background-color: #e5e5f7;
      opacity: 0.8;
      background-image:  linear-gradient(#444cf7 2px, transparent 2px), linear-gradient(90deg, #444cf7 2px, transparent 2px), linear-gradient(#444cf7 1px, transparent 1px), linear-gradient(90deg, #444cf7 1px, #e5e5f7 1px);
      background-size: 50px 50px, 50px 50px, 10px 10px, 10px 10px;
      background-position: -2px -2px, -2px -2px, -1px -1px, -1px -1px;
    }

    .box {
      margin-left: 50px;
      margin-right: 50px;
      margin-top: 30px;
      padding-left: 20px;
      padding-right: 20px;
      padding-bottom: 10px;
	  border: 2px solid black;
      border-radius: 5px;
      background-color: #ffffff;
    }

    .status-box {
      margin-left: 50px;
      margin-right: 50px;
      margin-bottom: 10px;
	  border: 2px solid black;
      border-radius: 5px;
      background-color: #ffffff;
    }

  </style>
</head>
<body>

  <div class="box">
    <h1>Ez80 Flash Utility</h1>
    <h4>Version: %FIRMWARE%</h4>
  </div>

  <div class="box">

    <h4>Files:</h4>

    <p>Free Storage: <span id="freespiffs">%FREESPIFFS%</span></p>
    <p>Used Storage: <span id="usedspiffs">%USEDSPIFFS%</span></p>
    <p>Total Storage: <span id="totalspiffs">%TOTALSPIFFS%</span></p>

  
    <p>
    <button onclick="listFilesButton()">List Files</button>
    <button onclick="showUploadButtonFancy()">Upload File</button>
    </p>

    <p id="upload_header"></p>
    <p id="upload_cancel"></p>
    <p id="upload_form"></p>
    <p id="details_header"></p>
    <p id="details_files"></p>
  
  </div>

  <div class="box">
    <h4>Status:</h4>
    <p id="status"></p>
  </div>
  
  <div class="box">
    <h4>ZDI Link Status:</h4>
    <div class="status-box" id="link_status_details"></div>
  </div>

  <div class="box">
    <h4>ZDI Utils:</h4>
    <button onclick="rebootButton()">Reboot</button>
  </div>

  <div class="box">
    <h4>System:</h4>
    <button onclick="logoutButton()">Logout</button>
  </div>


<script>

var pause_check = false;

function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}

function rebootButton() {
  document.getElementById("status").innerHTML = "Invoking Reboot ...";
  pause_check = true;
  // var xhr = new XMLHttpRequest();
  xmlhttp = new XMLHttpRequest();
  xmlhttp.open("GET", "/reboot", false);
  xmlhttp.send();
  document.getElementById("status").innerHTML = "Reboot OK!";
  <!-- window.open("/reboot","_self"); -->
  pause_check = false;
}

function listFilesButton() {
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listFiles", false);
  xmlhttp.send();
  document.getElementById("details_files").innerHTML = xmlhttp.responseText;
}

function checkZDILink() {
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/checkLink", false);
  xmlhttp.send();
  document.getElementById("link_status_details").innerHTML = xmlhttp.responseText;
}

function downloadDeleteButton(filename, action) {
  var urltocall = "/file?name=/" + filename + "&action=" + action;
  xmlhttp=new XMLHttpRequest();
  if (action == "delete") {
    xmlhttp.open("GET", urltocall, false);
    xmlhttp.send();
    document.getElementById("status").innerHTML = xmlhttp.responseText;
    xmlhttp.open("GET", "/listLiles", false);
    xmlhttp.send();
    document.getElementById("details_files").innerHTML = xmlhttp.responseText;
  }
  if (action == "download") {
    document.getElementById("status").innerHTML = "";
    window.open(urltocall,"_blank");
  }
}

function flashFile(filename, area) {

  var urltocall = "/flash_file?name=/" + filename + "&area=" + area;

  xmlhttp=new XMLHttpRequest();

  xmlhttp.open("GET", urltocall, false);
  xmlhttp.send();

}

function showUploadButtonFancy() {
  document.getElementById("upload_header").innerHTML = "<h3>Upload File<h3>";
  document.getElementById("upload_cancel").innerHTML = "<button onclick=\"cancelUpload()\">Cancel</button>";
  
  var uploadform = "<form method = \"POST\" action = \"/\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"data\"/><input type=\"submit\" name=\"upload\" value=\"Upload\" title = \"Upload File\"></form>"
  
  document.getElementById("upload_form").innerHTML = uploadform;
  
  var uploadform =
  "<form id=\"upload_form\" enctype=\"multipart/form-data\" method=\"post\">" +
  "<input type=\"file\" name=\"file1\" id=\"file1\" onchange=\"uploadFile()\"><br>" +
  "<progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>" +
  "<h3 id=\"status\"></h3>" +
  "<p id=\"loaded_n_total\"></p>" +
  "</form>";
  
  document.getElementById("upload_form").innerHTML = uploadform;
}

function cancelUpload() {
  document.getElementById("upload_header").innerHTML = ""
  document.getElementById("upload_cancel").innerHTML = "";
  document.getElementById("upload_form").innerHTML = "";
}

function _(el) {
  return document.getElementById(el);
}

function uploadFile() {
  var file = _("file1").files[0];
  // alert(file.name+" | "+file.size+" | "+file.type);
  var formdata = new FormData();
  formdata.append("file1", file);
  var ajax = new XMLHttpRequest();
  ajax.upload.addEventListener("progress", progressHandler, false);
  ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
  ajax.addEventListener("error", errorHandler, false);
  ajax.addEventListener("abort", abortHandler, false);
  ajax.open("POST", "/");
  ajax.send(formdata);
}

function progressHandler(event) {
  //_("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total; // event.total doesnt show accurate total file size
  _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes";
  var percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = Math.round(percent) + "% uploaded... please wait";
  if (percent >= 100) {
    _("status").innerHTML = "Please wait, writing file to filesystem";
  }
}

function completeHandler(event) {
  _("status").innerHTML = "Upload Complete";
  _("progressBar").value = 0;
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listFiles", false);
  xmlhttp.send();
  document.getElementById("status").innerHTML = "File Uploaded";
  document.getElementById("details_header").innerHTML = "<h3>Files<h3>";
  document.getElementById("details_files").innerHTML = xmlhttp.responseText;
}

function errorHandler(event) {
  _("status").innerHTML = "Upload Failed";
}

function abortHandler(event) {
  _("status").innerHTML = "inUpload Aborted";
}

document.addEventListener("DOMContentLoaded", (event) => {
  /* console.log("DOM fully loaded and parsed"); */

  document.getElementById("status").innerHTML = "";

  document.getElementById("details_header").innerHTML = "";
  document.getElementById("details_files").innerHTML = "";

  document.getElementById("upload_header").innerHTML = ""
  document.getElementById("upload_cancel").innerHTML = "";
  document.getElementById("upload_form").innerHTML = "";

  checkZDILink();
  
  var link_check = setInterval(function(){
	if(!pause_check) {
    	checkZDILink();
    }
  }, 3000);


}, false);

</script>


</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><a href="/">Log Back In</a></p>
</body>
</html>
)rawliteral";

// reboot.html base upon https://gist.github.com/Joel-James/62d98e8cb3a1b6b05102
const char reboot_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
<h3>
  Rebooting, returning to main page in <span id="countdown">30</span> seconds
</h3>
<script type="text/javascript">

  var seconds = 20;

  function countdown() {
    seconds = seconds - 1;
    if (seconds < 0) {
      window.location = "/";
    } else {
      document.getElementById("countdown").innerHTML = seconds;
      window.setTimeout("countdown()", 1000);
    }
  }

  countdown();

</script>
</body>
</html>
)rawliteral";


#endif // WEBPAGES_H
