<HTML>
<HEAD>
<TITLE>legOS Networking Protocol Daemon</TITLE>
</HEAD>

<?php
// Code graciously donated by Jeremy Katz of Duke U. Linux User's Group
// To modify text, just scroll down a little bit :)
   
function GetDirArray($sPath) { 
	//Load Directory Into Array 
	$handle=opendir($sPath); 
	while ($file = readdir($handle)) 
		$retVal[count($retVal)] = $file; //Clean up and sort 
	closedir($handle); 
	sort($retVal); 
	return $retVal; 
}

?>
<h3>Description</h3>
The LNPD and associated files are intended to allow communication of a program hosted on a Linux PC with a legOS program on an RCX. 
<h3>Documentation</h3>
Documentation for LNPD is unfortunately sparse. Your best bets are to read the README files below, and to poke around the archives at <a href="http://www.lugnet.com/robotics/rcx/legos/">lugnet.com</a> before asking questions in that forum.  
<h3>LNPD-related files</h3>
The file listing below is auto-generated, showing the current list of files in this directory. To get more information about them, try the links above or download them yourself.
 
<?php
$array = GetDirArray("./");

echo "<ul>\n";
for($i=0; $array[$i]; $i++) {
	if ($array[$i] == "." || $array[$i] == ".." || stristr($array[$i], "index.php")) 
		continue;
	echo "<li><a href=\"$array[$i]\">$array[$i]</a>\n";
}
echo "</ul>\n";

?>
</BODY>
</HTML>



