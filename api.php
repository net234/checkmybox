<?php
header("Content-Type: application/json");
$IP=$_SERVER["REMOTE_ADDR"];
$UA=$_SERVER["HTTP_USER_AGENT"];
$JSON=$_POST["json"];
if ($JSON === null) {
  $JSON=$_GET["json"];
}
$action=$_GET["action"];
$nodeName=$_GET["node"];
$err=0;
$param=array();
if ($JSON !== null) $param=json_decode($JSON,true);

//var_dump($param);
if ($err == 0) {
// API Handler - debut
  $err = 2;
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
  };

// API Handler - fin
}


if ($err == 0) { 
  $reponse = array(
    'status' => true,
    'message' => "Ok",
    'answer' => $answer,
    'parametre' => $param,
   );

} else { 
  $reponse = array(
    'status' => false,
    'message' =>"Error($err)",
    'parametre' => $param,
	'action' => $action,
    'IP' => $IP,
    'USER_AGENT' => $UA
  );
};
 
  echo json_encode($reponse);
?> 