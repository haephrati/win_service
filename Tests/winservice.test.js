const fs = require('fs');
const path = require('path');

function fail(msg) {
  console.error('FAIL ' + msg);
  process.exit(1);
}

const src = fs.readFileSync(path.join(__dirname, '..', 'SG_WinService.h'), 'utf8');
if (!src.includes('SG_WinService.exe')) fail('exe');
if (!src.includes('SG Win Service')) fail('display');
if (!src.includes('MAIN_TIMER_ID')) fail('timer');
if (!src.includes('DATE_FORMAT')) fail('date');
console.log('OK WinServiceTests');
