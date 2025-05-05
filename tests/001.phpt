--TEST--
Check if neptune is loaded
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
?>
--FILE--
<?php
echo 'The extension "neptune" is available';
?>
--EXPECT--
The extension "neptune" is available
