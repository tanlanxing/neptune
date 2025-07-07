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
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt 2>&1';
	exec($cmd, $output, $status);
	if ($status !=0) {
		print_r($output);
		echo 'skip';
		return;
	}
	$args = [
		[$currPath . '/test.tar', 'r'],
		[$currPath . '/test.tar', 'r', true]
	];
	foreach($args as $arg) {
		$tar = new Tar(...$arg);
		unset($tar);
	}
	unlink($currPath . '/test.tar');
	echo 'OK';
}

test();
?>
--EXPECT--
OK