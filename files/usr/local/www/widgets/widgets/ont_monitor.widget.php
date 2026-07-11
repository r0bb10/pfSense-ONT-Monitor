<?php

require_once('guiconfig.inc');
require_once('ont-monitor.inc');

if (isset($_POST['widgetkey']) || isset($_GET['widgetkey'])) {
	$requested_widgetkey = $_POST['widgetkey'] ?? $_GET['widgetkey'];
	[$widget_name, $widget_id] = array_pad(explode('-', $requested_widgetkey, 2), 2, null);
	if ($widget_name === basename(__FILE__, '.widget.php') && is_numericint($widget_id)) {
		$widgetkey = $requested_widgetkey;
	} else {
		print gettext('Invalid Widget Key');
		exit;
	}
}

if (!isset($widgetkey)) {
	print gettext('Missing Widget Key');
	exit;
}

function ont_monitor_widget_rows() {
	$data = ont_monitor_fetch();
	if (isset($data['error'])) {
		return '<tr><td><span class="text-danger">' . ont_monitor_escape($data['error']) . '</span></td></tr>';
	}

	$rows = [
		[gettext('GPON state'), $data['gpon']['state']],
		[gettext('Optical receive'), $data['gpon']['rx_power_dbm'] . ' dBm'],
		[gettext('Optical transmit'), $data['gpon']['tx_power_dbm'] . ' dBm'],
		[gettext('Module temperature'), $data['gpon']['temperature_c'] . ' C'],
		[gettext('Module voltage'), $data['gpon']['voltage_v'] . ' V'],
		[gettext('Laser bias'), $data['gpon']['bias_ma'] . ' mA'],
		[gettext('Ethernet'), $data['ethernet']['status']],
		[gettext('Ethernet errors'), $data['ethernet']['rx_errors'] . ' RX / ' . $data['ethernet']['tx_errors'] . ' TX'],
	];

	$html = '';
	foreach ($rows as [$label, $value]) {
		$html .= '<tr><th>' . ont_monitor_escape($label) . '</th><td>' . ont_monitor_escape($value ?? '-') . '</td></tr>';
	}
	return $html;
}

if (isset($_POST['ajax'])) {
	print ont_monitor_widget_rows();
	exit;
}

$settings = ont_monitor_settings();
?>
<div class="table-responsive">
	<table class="table table-striped table-hover table-condensed">
		<tbody id="<?=ont_monitor_escape($widgetkey)?>">
			<?=ont_monitor_widget_rows()?>
		</tbody>
	</table>
</div>
<div class="text-right">
	<a href="/status_ont_monitor.php"><?=gettext('Full status')?></a>
</div>

<script type="text/javascript">
events.push(function() {
	function ontMonitorCallback(response) {
		$(<?=json_encode('#' . $widgetkey)?>).html(response);
	}

	var refreshObject = new Object();
	refreshObject.name = 'ont_monitor';
	refreshObject.url = '/widgets/widgets/ont_monitor.widget.php';
	refreshObject.callback = ontMonitorCallback;
	refreshObject.parms = {
		ajax: 'ajax',
		widgetkey: <?=json_encode($widgetkey)?>
	};
	refreshObject.freq = <?=json_encode($settings['refresh_seconds'])?>;
	register_ajax(refreshObject);
});
</script>
