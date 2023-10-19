<?php 
/*************************************************
 *************************************************
    api.php   small api interface for checkmybox and others examples of betaEvents arduino libraries
    can be use on any web server with PHP active
    see https://github.com/net234/checkmybox 
    Copyright 2021 Pierre HENRY net23@frdev.com All - right reserved.

  This file is part of betaEvents.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.

*****************/
header("Content-Type: application/json");
$IP=$_SERVER['REMOTE_ADDR'];        // IP of user (informative only)
$UA=$_SERVER['HTTP_USER_AGENT'];    // Client of user (informative only)

$action=$_REQUEST['action'];    // action to do  (mendatory)
$nodeName=$_REQUEST['node'];    // name of client (mendatory)

// first check to avoid most web scanners
if (strlen($action) < 4 || strlen($nodeName) < 4) {
  $reponse = array(
    'status' => false,
    'message' => 'Acces invalide',
    'IP' => $IP,
    'USER_AGENT' => $UA
  );
   echo json_encode($reponse);
   exit();  
}
// we suppose we have a correct api request here
$err=0;

// obtional parameter for the action (in JSON)
$paramStr = $_REQUEST['json'];
//if ($paramStr === null) {
//  $paramStr=$_REQUEST["json"]; // older version of checkmybox use this key
//}
$param=json_decode($paramStr,true);

//var_dump($param);


// API Handler - debut
$err = 2;
$message = 'Action invalide';

if ( $action == "test") {
    $err = 0;
    $aDate = date_create("2021/10/31 02:00");
    $timeZone = -date_offset_get($aDate)/3600;
    $data = file_get_contents("data/nodes.json");
    $base = json_decode($data,true);
    $base[$nodeName] = $param;
    $file = file_put_contents("data/nodes.json",json_encode($base),LOCK_EX);
    //echo $base["test"]["value"];
    
    
    $answer = array(
      'test' => "Ok",
      'timezone' => $timeZone,
      'file' => $file,
    );

  } else if ( $action == "timezone") {
    $err = 0;
    $timeZone = -date_offset_get(date_create())/3600;
    $data = file_get_contents("data/nodes.json");
    $base = json_decode($data,true);
    $base[$nodeName] = $param;
    $file = file_put_contents("data/nodes.json",json_encode($base),LOCK_EX);    
    $answer = array(
      'timezone' => $timeZone,
    );
  } else if ( $action == "checknode") {
    $err = 0;
    $data = file_get_contents("data/nodes.json");
    $base = json_decode($data,true);
    $nodeTime = $base[$nodeName]['timestamp'] + ($base[$nodeName]['timeZone']*3600);
    $delta = ( time() - $nodeTime )/60;
    $result = $delta < 5.5; 
    $answer = array(
      'result' => $result,
      'delta' => $delta,
    );
   } else if ( $action == "clearnode") {
     $err = 0;
     $data = file_get_contents("data/nodes.json");
     $base = json_decode($data,true);
     $result = array_key_exists($nodeName,$base);
     if ($result) { 
       unset($base[$nodeName]);
       file_put_contents("data/nodes.json",json_encode($base),LOCK_EX);
     }
    $answer = array(
      'result' => $result,
    );
  };

// API Handler - fin


if ($err == 0) { 
  $reponse = array(
    'status' => true,
    'message' => "Ok",
    'answer' => $answer,
    //'parametre' => $param,
   );

} else {

  if ($message == "") $message = "Error($err)";
  $reponse = array(
    'status' => false,
    'err' => $err,
    'message' => $message,
    'parametres' => $param,
    'action' => $action,
    'IP' => $IP,
    'USER_AGENT' => $UA
  );
};
 
  echo json_encode($reponse);
?>