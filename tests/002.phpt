--TEST--
Check Tar::__construct
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
?>
--FILE--
<?php
use Neptune\Archive\Tar;

function test() {
	$currPath = dirname(realpath(__file__));
	if (file_exists($currPath . '/test.tar')) {
        unlink($currPath . '/test.tar');
    }
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt';
	exec($cmd, $output, $status);
	if ($status !=0) {
		echo 'skip';
		return;
	}
	$tar = new Tar($currPath . '/test.tar', 'r');
	unset($tar);
	unlink($currPath . '/test.tar');
	echo 'OK';
}

test();
?>
--EXPECT--
OK