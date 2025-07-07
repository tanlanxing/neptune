--TEST--
Check Neptune\Archive\Tar::addFrom
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
?>
--FILE--
<?php
use Neptune\Archive\Tar;

function test_Neptune_Archive_Tar_addFrom() {
	clean();
	$currPath = dirname(realpath(__file__));	
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt 2>&1';
	exec($cmd, $output, $status);
	if ($status !=0) {
		print_r($output);
		echo 'skip';
		return;
	}

	$args = [
		false,
		true
	];
	foreach($args as $arg) {
		$srcTar = new Tar($currPath . '/test.tar', 'r', $arg);
		$newTar = new Tar($currPath . '/new.tar', 'w', $arg);
		$newTar->addFrom($srcTar, '001.phpt');
		unset($srcTar, $newTar);

		if (!file_exists($currPath . '/out')) {
			if (!mkdir($currPath . '/out', 0755, true)) {
				echo 'skip';
				return;
			}
		}

		$cmd = 'cd ' . $currPath . '; tar -xf new.tar -C out/ 2>&1';
		unset($output);
		exec($cmd, $output, $status);
		if ($status !=0) {
			print_r($output);
			echo 'skip';
			return;
		}

		if (md5_file($currPath . '/001.phpt') == md5_file($currPath . '/out/001.phpt')) {
			echo 'OK';
		}
		unlink($currPath . '/new.tar');
	}
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

test_Neptune_Archive_Tar_addFrom();
?>
--EXPECT--
OKOK