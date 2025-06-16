--TEST--
Check Neptune\Archive\Tar::addFrom
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
// Used to test for memory leaks, which can take a long time
echo 'skip';
?>
--FILE--
<?php
use Neptune\Archive\Tar;

function test_Neptune_Archive_Tar_addFrom() {
	clean();
	$currPath = dirname(realpath(__file__));	
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt 003.phpt 004.phpt 005.phpt 006.phpt 2>&1';
	exec($cmd, $output, $status);
	if ($status !=0) {
		print_r($output);
		echo 'skip';
		return;
	}

	$tar = new Tar($currPath . '/test.tar', 'r');
	foreach(range(1, 6) as $i) {
		$filename = sprintf('%03d.phpt', $i);
		$target = $currPath . '/out/' . $filename;
		$tar->extractFile($filename, $target);

	}
	unset($tar);

	$srcTar = new Tar($currPath . '/test.tar', 'r');
	$newTar = new Tar($currPath . '/new.tar', 'w');
	foreach(range(1, 6) as $i) {
		$filename = sprintf('%03d.phpt', $i);
		$newTar->addFrom($srcTar, '001.phpt');
	}
	unset($srcTar, $newTar);
	clean();
}

function clean() {
	$currPath = dirname(realpath(__file__));
	if (file_exists($currPath . '/test.tar')) {
        unlink($currPath . '/test.tar');
    }
	if (file_exists($currPath . '/new.tar')) {
        unlink($currPath . '/new.tar');
    }

	if (file_exists($currPath . '/out')) {
		$cmd = 'rm -rf ' . $currPath . '/out 2>&1';
		exec($cmd, $output, $status);
		if ($status !=0) {
			print_r($output);
			echo 'skip';
			return;
		}
	}
}
for($i = 0; $i < 10000; $i++) {
	test_Neptune_Archive_Tar_addFrom();
	echo memory_get_usage(), PHP_EOL;
}
echo 'OK';
?>
--EXPECT--
OK