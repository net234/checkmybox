<?php
header("Content-Type: application/json");
$IP=$_SERVER["REMOTE_ADDR"];
$UA=$_SERVER["HTTP_USER_AGENT"];
$JSON=$_POST["JSON"];
if ($JSON === null) {
  $JSON=$_GET["JSON"];
}
$err=0;
$param=array();
if ($JSON === null) {
  $param['action']=$_GET["action"];
} else {
  try {
    $param=json_decode($JSON,true);
    } catch (Exception $e) {
      $err = 1;
    }
}
//var_dump($param);
if ($err == 0) {
// API Handler - debut
  $err = 2;
  if ( $param['action'] == "test") {
    $err = 0;	
	$answer = array(
	  'test' => "Ok",
	);

  } else if ( $param['action'] == "timezone") {
    $err = 0;
	$answer = array(
	  'timezone' => -2,
    );
  };

// API Handler - fin
}


if ($err == 0) { 
  $reponse = array(
    'status' => true,
    'message' => "Ok",
    'answer' => $answer,
   );

} else { 
  $reponse = array(
    'status' => false,
    'message' =>"Error($err)",
    'parametre' => $param,
    'IP' => $IP,
    'USER_AGENT' => $UA
  );
};
 
echo json_encode($reponse);
$headers = "From: robot_hgotgs@frdev.com\r\n";
mail("pierre@musictrad.org",utf8_decode("appel HGOTGS API"),utf8_decode("API CMD=$CMD P1=$P1  P2=$P2 P3=$P3 P4=$P4\r\nIP=$IP\r\nnav=$UA\r\n"),$headers );
?> 