<?php

require_once('guiconfig.inc');
require_once('ont-monitor.inc');

if (isset($_GET['ajax'])) {
	header('Content-Type: application/json');
	header('Cache-Control: no-store');
	echo json_encode(ont_monitor_fetch());
	exit;
}

$shortcut_section = 'ont_monitor';
$pgtitle = [gettext('Status'), gettext('ONT Monitor')];
include('head.inc');
?>

<ul class="nav nav-tabs">
	<li><a href="/pkg_edit.php?xml=ont-monitor.xml"><?=gettext('Settings')?></a></li>
	<li class="active"><a href="/status_ont_monitor.php"><?=gettext('Status')?></a></li>
</ul>

<style>
.ont-monitor-table {
	table-layout: fixed;
}
.ont-monitor-table th {
	width: 35%;
}
</style>

<div id="ont-monitor-error" class="alert alert-danger" style="display: none;"></div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title"><?=gettext('GPON')?></h2></div>
	<div class="panel-body">
		<table class="table table-striped table-condensed ont-monitor-table">
			<tbody>
				<tr><th><?=gettext('State')?></th><td id="ont-gpon-state">-</td></tr>
				<tr><th><?=gettext('Optical receive')?></th><td id="ont-rx-power">-</td></tr>
				<tr><th><?=gettext('Optical transmit')?></th><td id="ont-tx-power">-</td></tr>
				<tr><th><?=gettext('Temperature')?></th><td id="ont-temperature">-</td></tr>
				<tr><th><?=gettext('Voltage')?></th><td id="ont-voltage">-</td></tr>
				<tr><th><?=gettext('Laser bias')?></th><td id="ont-bias">-</td></tr>
			</tbody>
		</table>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title"><?=gettext('Device')?></h2></div>
	<div class="panel-body">
		<table class="table table-striped table-condensed ont-monitor-table">
			<tbody>
				<tr><th><?=gettext('Model')?></th><td id="ont-model">-</td></tr>
				<tr><th><?=gettext('Hardware')?></th><td id="ont-hardware">-</td></tr>
				<tr><th><?=gettext('Software')?></th><td id="ont-software">-</td></tr>
				<tr><th><?=gettext('Boot loader')?></th><td id="ont-bootloader">-</td></tr>
				<tr><th><?=gettext('PON serial')?></th><td id="ont-pon-serial">-</td></tr>
			</tbody>
		</table>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title"><?=gettext('Ethernet')?></h2></div>
	<div class="panel-body">
		<table class="table table-striped table-condensed ont-monitor-table">
			<tbody>
				<tr><th><?=gettext('Status')?></th><td id="ont-ethernet-status">-</td></tr>
				<tr><th><?=gettext('MAC address')?></th><td id="ont-ethernet-mac">-</td></tr>
				<tr><th><?=gettext('Errors')?></th><td id="ont-ethernet-errors">-</td></tr>
			</tbody>
		</table>
	</div>
</div>

<script>
async function refreshOntMonitor() {
	const response = await fetch('/status_ont_monitor.php?ajax=1', {cache: 'no-store'});
	const data = await response.json();
	const error = document.getElementById('ont-monitor-error');
	if (data.error) {
		error.textContent = data.error;
		error.style.display = 'block';
		return;
	}
	error.style.display = 'none';
	document.getElementById('ont-gpon-state').textContent = data.gpon.state || '-';
	document.getElementById('ont-rx-power').textContent = `${data.gpon.rx_power_dbm || '-'} dBm`;
	document.getElementById('ont-tx-power').textContent = `${data.gpon.tx_power_dbm || '-'} dBm`;
	document.getElementById('ont-temperature').textContent = `${data.gpon.temperature_c || '-'} C`;
	document.getElementById('ont-voltage').textContent = `${data.gpon.voltage_v || '-'} V`;
	document.getElementById('ont-bias').textContent = `${data.gpon.bias_ma || '-'} mA`;
	document.getElementById('ont-model').textContent = data.device.model || '-';
	document.getElementById('ont-hardware').textContent = data.device.hardware || '-';
	document.getElementById('ont-software').textContent = data.device.software || '-';
	document.getElementById('ont-bootloader').textContent = data.device.bootloader || '-';
	document.getElementById('ont-pon-serial').textContent = data.device.pon_serial || '-';
	document.getElementById('ont-ethernet-status').textContent = data.ethernet.status || '-';
	document.getElementById('ont-ethernet-mac').textContent = data.ethernet.mac || '-';
	document.getElementById('ont-ethernet-errors').textContent = `${data.ethernet.rx_errors || '-'} RX / ${data.ethernet.tx_errors || '-'} TX`;
}

refreshOntMonitor();
setInterval(refreshOntMonitor, <?=json_encode(ont_monitor_settings()['refresh_seconds'] * 1000)?>);
</script>

<?php include('foot.inc'); ?>
