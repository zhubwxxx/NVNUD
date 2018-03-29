#include "appinfo.h"


UInt8 APP_VERSION[4] = {1,2,1,0};
UInt8 NewVersion[APP_NUM][4];
int SERVER;
bool UFLASH_FLAG = false;            //升级方式标志 true-U盘升级 false-运营平台升级
bool DEBUG_FLAG = false;             /*命令行执行时是否打开调试模式*/

QString UpdateCfg = "updateCfg.ini";
QString BackupDir = "backup";
QString TempDir = "temp";
QString UpdatePackage = "";
QString UpdateSumary = "";
QString Md5Cfg = "md5.ini";
QString IpCfg = "ip.ini";

QString FtpUser = " ";             //ftp用户名
QString FtpPass = " ";             //ftp密码
QString FtpPath = " ";             //ftp路径

QString BillingCtrlDir = " ";
QString GatewayDir = " ";
QString MatrixCtrlDir = " ";
QString ConfigDir = " ";
QString DebugDir = " ";
QString UpdateDir = " ";
QString DaemonDir = " ";
QString ChargeOperateDir = " ";
QString ChargeCtrlDir = " ";

QString BillingCtrlName = " ";
QString GatewayName = " ";
QString MatrixCtrlName = "";
QString ConfigName = " ";
QString DebugName = " ";
QString UpdateName = " ";
QString DaemonName = " ";
QString ChargeOperateName = " ";
QString ChargeCtrlName = " ";

/*诊断参数*/
QString DebugArg = "NULL";
QString DebugArg_QueryStakeInfo = "QueryStakeInfo";
QString DebugArg_DownloadFile = "DownloadFile";
QString DebugArg_UpdateInfo = "UpdateInfo";

