const TIMEZONE = "Asia/Kolkata";

/* ===========================================================
   A. WEB APP (RFID ENTRY)
=========================================================== */

function doGet(e) {
  const rfid = (e?.parameter?.rfid || "").trim();
  if (!rfid) return ContentService.createTextOutput("ERROR|No RFID");

  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const studentsSheet = ss.getSheetByName("StudentList");
  const attendanceSheet = ss.getSheetByName("AttendanceLog");
  const timetableSheet = ss.getSheetByName("TimeTable");
  const holidaysSheet = ss.getSheetByName("Holidays");

  if (!studentsSheet || !attendanceSheet || !timetableSheet) {
    return ContentService.createTextOutput("ERROR|Missing sheets");
  }

  const now = new Date();

  if (holidaysSheet && isTodayHoliday(holidaysSheet, now)) {
    return ContentService.createTextOutput("HOLIDAY");
  }

  const student = getStudentData(studentsSheet, rfid);
  if (!student) return ContentService.createTextOutput("ERROR|Not Found");

  // 🚨 Prevent double scan
  if (isDuplicateScan(attendanceSheet, student.rollNo, now)) {
    return ContentService.createTextOutput("DUPLICATE");
  }

  const subject = getCurrentSubject(timetableSheet, now);

  const dateStr = Utilities.formatDate(now, TIMEZONE, "dd/MM/yyyy");
  const timeStr = Utilities.formatDate(now, TIMEZONE, "HH:mm");

  if (subject === "NA") {
    writeRow(attendanceSheet, [dateStr, timeStr, rfid, student.name, student.rollNo, "N/A", "NONE"]);
    return ContentService.createTextOutput("OUTSIDE");
  }

  const lastState = getLastScanState(attendanceSheet, student.rollNo, subject, now);
  const scanType = (lastState === "IN") ? "OUT" : "IN";

  writeRow(attendanceSheet, [
    dateStr, timeStr, rfid,
    student.name, student.rollNo,
    subject, scanType
  ]);

  return ContentService.createTextOutput(`OK|${student.rollNo}|${scanType}`);
}

/* ===========================================================
   B. SUMMARY (FIXED LOGIC)
=========================================================== */

function createBigSummaryTable() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const logSheet = ss.getSheetByName("AttendanceLog");
  const sheet = ss.getSheetByName("BigSummary") || ss.insertSheet("BigSummary");

  const data = logSheet.getDataRange().getValues();
  if (data.length < 2) return;

  // 🎯 SUBJECT LIST (from your timetable)
  const subjectsList = [
    "M&MI", "EM-1", "EEL", "EMML", "ES", "ED",
    "WEB DESIGNING", "MENTOR", "EEML", "EE",
    "EMEC", "MP-1A", "M&ML", "EMEC T"
  ];

  let map = {};
  let subjectSessions = {};

  // 📌 PROCESS DATA
  for (let i = 1; i < data.length; i++) {
    const date = data[i][0];
    const time = data[i][1];
    const roll = String(data[i][4]).trim();
    const name = data[i][3];
    const subject = data[i][5];
    const status = data[i][6];

    if (!roll || !subject) continue;

    const sessionKey = date + "_" + subject;

    // Track sessions per subject
    if (!subjectSessions[subject]) {
      subjectSessions[subject] = new Set();
    }
    subjectSessions[subject].add(sessionKey);

    // Init student
    if (!map[roll]) {
      map[roll] = {
        name: name,
        subjects: {}
      };
    }

    // Init subject
    if (!map[roll].subjects[subject]) {
      map[roll].subjects[subject] = {};
    }

    // Init session
    if (!map[roll].subjects[subject][sessionKey]) {
      map[roll].subjects[subject][sessionKey] = {
        inTime: null,
        outTime: null
      };
    }

    if (status === "IN") {
      map[roll].subjects[subject][sessionKey].inTime = time;
    }

    if (status === "OUT") {
      map[roll].subjects[subject][sessionKey].outTime = time;
    }
  }

  // 📊 HEADER
  let header = ["Name", "Roll No", ...subjectsList, "Overall %", "Status"];
  let output = [header];

  // 📊 BUILD DATA
  for (let roll in map) {
    let student = map[roll];
    let row = [student.name, roll];

    let totalPercent = 0;
    let subjectCount = 0;

    for (let subject of subjectsList) {
      if (!subjectSessions[subject]) {
        row.push("0%");
        continue;
      }

      let sessions = student.subjects[subject] || {};
      let total = subjectSessions[subject].size;
      let attended = 0;

      for (let session in sessions) {
        let s = sessions[session];

        if (!s.inTime) continue;

        if (!s.outTime) {
          attended++;
        } else {
          const inMin = timeToMinutes(s.inTime);
          const outMin = timeToMinutes(s.outTime);

          if ((outMin - inMin) >= 30) {
            attended++;
          }
        }
      }

      let percent = total > 0 ? (attended / total) * 100 : 0;

      row.push(percent.toFixed(0) + "%");

      totalPercent += percent;
      subjectCount++;
    }

    let overall = subjectCount > 0
      ? (totalPercent / subjectCount).toFixed(2)
      : 0;

    let status = "Good";
    if (overall < 50) status = "Critical";
    else if (overall < 75) status = "Warning";

    row.push(overall + "%");
    row.push(status);

    output.push(row);
  }

  sheet.clear();
  sheet.getRange(1, 1, output.length, output[0].length).setValues(output);

  Logger.log("Wide Summary Table Created");
}

function timeToMinutes(t) {
  if (!t) return 0;

  if (Object.prototype.toString.call(t) === "[object Date]") {
    return t.getHours() * 60 + t.getMinutes();
  }

  if (typeof t === "string" && t.includes(":")) {
    const [h, m] = t.split(":").map(Number);
    if (!isNaN(h) && !isNaN(m)) {
      return h * 60 + m;
    }
  }

  return 0;
}

/* ===========================================================
   C. DASHBOARDS
=========================================================== */

function createStudentDashboard() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sheet = ss.getSheetByName("StudentDashboard");

  if (!sheet) sheet = ss.insertSheet("StudentDashboard");
  else sheet.clear();

  const source = ss.getSheetByName("BigSummary");
  const data = source.getDataRange().getValues();

  if (data.length < 2) return;

  const header = data[0];
  let rows = data.slice(1);

  // 📊 SORT by Roll No
  rows.sort((a, b) => Number(a[1]) - Number(b[1]));

  // 📥 WRITE DATA
  sheet.getRange(1, 1, 1, header.length).setValues([header]);
  sheet.getRange(2, 1, rows.length, header.length).setValues(rows);

  // 🎨 COLOR CODING
  for (let i = 0; i < rows.length; i++) {
    for (let j = 2; j < header.length; j++) { // skip Name & Roll
      let val = rows[i][j];
      if (!val) continue;

      // extract number from "75%"
      let num = parseFloat(String(val).replace("%", ""));
      let cell = sheet.getRange(i + 2, j + 1);

      if (num >= 75) {
        cell.setBackground("#b7e1cd"); // green
      } else if (num >= 50) {
        cell.setBackground("#fff2cc"); // yellow
      } else {
        cell.setBackground("#f4c7c3"); // red
      }
    }
  }

  // 🔧 OPTIONAL FORMATTING
  sheet.getRange(1, 1, 1, header.length).setFontWeight("bold");
  sheet.autoResizeColumns(1, header.length);
  sheet.setFrozenRows(1);

  Logger.log("Student Dashboard Ready (No Search)");
}

function createTeacherDashboard() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sheet = ss.getSheetByName("TeacherDashboard");

  if (!sheet) sheet = ss.insertSheet("TeacherDashboard");
  else sheet.clear();

  const source = ss.getSheetByName("BigSummary");
  const data = source.getDataRange().getValues();
  if (data.length < 2) return;

  const header = data[0];
  const rows = data.slice(1);

  let totalStudents = rows.length;
  let totalPercent = 0;
  let good = 0, warning = 0, critical = 0;
  let defaulters = [];

  // ===== Calculate metrics =====
  rows.forEach(row => {
    // Average % across subjects for this student
    let sum = 0, count = 0;
    for (let i = 2; i < row.length; i++) { // skip Roll & Name
      let val = parseFloat(String(row[i]).replace("%",""));
      if (!isNaN(val)) { sum += val; count++; }
    }
    let avg = count > 0 ? sum / count : 0;
    totalPercent += avg;

    if (avg >= 75) good++;
    else if (avg >= 50) { warning++; defaulters.push([row[1], row[0], avg.toFixed(2) + "%"]); }
    else { critical++; defaulters.push([row[1], row[0], avg.toFixed(2) + "%"]); }
  });

  let avgAttendance = (totalPercent / totalStudents).toFixed(2);

  // ===== HEADER =====
  sheet.getRange("A1").setValue("📊 TEACHER DASHBOARD")
       .setFontWeight("bold").setFontSize(16);

  // ===== METRICS =====
  sheet.getRange("A3:B7").setValues([
    ["Total Students", totalStudents],
    ["Average Attendance %", avgAttendance + "%"],
    ["Good (≥75%)", good],
    ["Warning (50–74%)", warning],
    ["Critical (<50%)", critical]
  ]);

  // ===== PIE CHART =====
  // Dynamically create a temp range for chart
  const chartData = [["Category","Count"]];
  chartData.push(["Good (≥75%)", good]);
  chartData.push(["Warning (50–74%)", warning]);
  chartData.push(["Critical (<50%)", critical]);

  const chartSheetRange = sheet.getRange("H1:I4");
  chartSheetRange.clearContent();
  chartSheetRange.offset(0,0,chartData.length,chartData[0].length).setValues(chartData);

  const chart = sheet.newChart()
    .setChartType(Charts.ChartType.PIE)
    .addRange(chartSheetRange)
    .setPosition(3, 4, 0, 0)
    .setOption("title", "Attendance Status Distribution")
    .setOption("pieSliceText", "value-and-percentage")
    .build();
  sheet.insertChart(chart);

  // ===== DEFAULTER LIST =====
  const startRow = 10;
  if (defaulters.length > 0) {
    sheet.getRange(startRow,1,1,3).setValues([["Name","Roll No","Average %"]])
         .setFontWeight("bold");

    sheet.getRange(startRow+1,1,defaulters.length,3).setValues(defaulters);

    // Color coding for defaulters
    defaulters.forEach((d, idx) => {
      const val = parseFloat(d[2].replace("%",""));
      let rowNum = startRow + 1 + idx;
      let cell = sheet.getRange(rowNum,3);
      if (val >= 75) cell.setBackground("#b7e1cd"); // green
      else if (val >= 50) cell.setBackground("#fff2cc"); // yellow
      else cell.setBackground("#f4c7c3"); // red
    });
  }

  sheet.autoResizeColumns(1, 6);
  sheet.setFrozenRows(1);

  Logger.log("Teacher Dashboard Ready with Pie Chart & Defaulters List");
}

/* ===========================================================
   D. REPORTS + ALERTS
=========================================================== */

function generateMonthlyReport() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const sheet = ss.getSheetByName("MonthlyReport") || ss.insertSheet("MonthlyReport");
  const data = ss.getSheetByName("AttendanceLog").getDataRange().getValues().slice(1);

  let map = {};

  data.forEach(r=>{
    const roll = r[4];
    const subject = r[5];
    const scan = r[6];
    const date = r[0];

    if(!roll || subject==="N/A") return;

    const month = date.split("/")[1];

    map[roll] = map[roll] || {};
    map[roll][month] = map[roll][month] || {p:0,t:new Set()};

    if(scan==="IN"){
      map[roll][month].p++;
      map[roll][month].t.add(date);
    }
  });

  sheet.clear();
  sheet.appendRow(["Roll","Month","%"]);

  for(let r in map){
    for(let m in map[r]){
      let obj = map[r][m];
      let per = (obj.p/obj.t.size)*100;
      sheet.appendRow([r,m,per.toFixed(2)]);
    }
  }
}

function generateLowAttendanceAlerts() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const sheet = ss.getSheetByName("Alerts") || ss.insertSheet("Alerts");

  const data = ss.getSheetByName("Summary").getDataRange().getValues().slice(1);

  sheet.clear();
  sheet.appendRow(["Roll","Name","%"]);

  data.forEach(r=>{
    if(parseFloat(r[4])<75){
      sheet.appendRow([r[0],r[1],r[4]]);
    }
  });
}

/* ===========================================================
   E. HELPERS
=========================================================== */

function writeRow(sheet,row){
  sheet.getRange(sheet.getLastRow()+1,1,1,row.length).setValues([row]);
}

function isDuplicateScan(sheet, roll, now){
  const lr = sheet.getLastRow();
  if(lr<2) return false;

  const last = sheet.getRange(lr,1,1,7).getValues()[0];
  if(String(last[4])!==roll) return false;

  const diff = (now - new Date())/1000;
  return diff<10;
}

function getStudentData(sheet,rfid){
  const d = sheet.getDataRange().getValues();
  for(let i=1;i<d.length;i++){
    if(String(d[i][0])===rfid){
      return {name:d[i][1],rollNo:d[i][2]};
    }
  }
  return null;
}

function getLastScanState(sheet,roll,subject,now){
  const today = Utilities.formatDate(now,TIMEZONE,"dd/MM/yyyy");
  const d = sheet.getRange("A2:G"+sheet.getLastRow()).getValues();

  for(let i=d.length-1;i>=0;i--){
    if(d[i][0]===today && d[i][4]==roll && d[i][5]==subject){
      return d[i][6]||"NONE";
    }
  }
  return "NONE";
}

function getCurrentSubject(sheet,now){
  const d = sheet.getDataRange().getValues();
  const day = Utilities.formatDate(now,TIMEZONE,"EEEE").toLowerCase();

  let r = d.findIndex(x=>String(x[0]).toLowerCase()===day);
  if(r==-1) return "NA";

  let nowMin = now.getHours()*60+now.getMinutes();

  for(let c=1;c<d[0].length;c++){
    let slot = d[0][c];
    if(!slot) continue;

    let [s,e] = slot.split("-");
    let sm = parseTime(s), em=parseTime(e);

    if(nowMin>=sm && nowMin<em){
      return d[r][c]||"NA";
    }
  }
  return "NA";
}

function parseTime(t){
  let [h,m]=t.split(":").map(Number);
  return h*60+m;
}

function isTodayHoliday(sheet,now){
  const today = Utilities.formatDate(now,TIMEZONE,"dd/MM/yyyy");
  const d = sheet.getDataRange().getValues();

  for(let i=1;i<d.length;i++){
    if(Utilities.formatDate(new Date(d[i][0]),TIMEZONE,"dd/MM/yyyy")===today){
      return true;
    }
  }
  return false;
}
