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

function ont_monitor_widget_value($value) {
	return $value === null || $value === '' ? '-' : ont_monitor_escape($value);
}

function ont_monitor_widget_gpon_icon($severity) {
	$icons = [
		'warning' => 'fa-arrow-right text-warning',
		'success' => 'fa-arrow-up text-success',
		'danger' => 'fa-arrow-down text-danger',
	];
	return $icons[$severity] ?? $icons['danger'];
}

function ont_monitor_widget_ethernet_value($status) {
	$value = ont_monitor_widget_value($status);
	if ($value === 'Down') {
		return 'Down<br>&nbsp;';
	}
	return str_replace(' &lt;', '<br>&lt;', $value);
}

function ont_monitor_widget_content() {
	$data = ont_monitor_fetch();
	if (isset($data['error'])) {
		return '<tr><td colspan="4" class="text-center text-danger">' . ont_monitor_escape($data['error']) . '</td></tr>';
	}

	$rx_errors = $data['ethernet']['rx_errors'];
	$tx_errors = $data['ethernet']['tx_errors'];
	$has_errors = (is_numeric($rx_errors) && (int)$rx_errors > 0) || (is_numeric($tx_errors) && (int)$tx_errors > 0);
	$error_class = $has_errors ? ' text-danger' : '';

	return '<tr>'
		. '<td colspan="2" class="text-center"><strong>' . gettext('Model:') . '</strong> ZTE F6005</td>'
		. '<td colspan="2" class="text-center"><strong>' . gettext('Software:') . '</strong> ' . ont_monitor_widget_value($data['device']['software']) . '</td>'
		. '</tr><tr>'
		. '<th class="text-center">' . gettext('GPON') . '</th><th class="text-center">' . gettext('Optics') . '</th><th class="text-center">' . gettext('Ethernet') . '</th><th class="text-center">' . gettext('Errors') . '</th>'
		. '</tr><tr>'
		. '<td class="text-center"><i class="fa ' . ont_monitor_widget_gpon_icon($data['gpon']['state_severity']) . '" aria-hidden="true"></i> '
		. ont_monitor_widget_value($data['gpon']['state']) . '<br>' . ont_monitor_widget_value($data['gpon']['state_label']) . '</td>'
		. '<td class="text-center"><strong>RX</strong> ' . ont_monitor_widget_value($data['gpon']['rx_power_dbm']) . ' dBm<br><strong>TX</strong> ' . ont_monitor_widget_value($data['gpon']['tx_power_dbm']) . ' dBm</td>'
		. '<td class="text-center">' . ont_monitor_widget_ethernet_value($data['ethernet']['status']) . '</td>'
		. '<td class="text-center' . $error_class . '"><strong>RX</strong> ' . ont_monitor_widget_value($rx_errors) . '<br><strong>TX</strong> ' . ont_monitor_widget_value($tx_errors) . '</td>'
		. '</tr>';
}

if (isset($_POST['ajax'])) {
	print ont_monitor_widget_content();
	exit;
}

$settings = ont_monitor_settings();
?>
<div class="table-responsive">
	<table class="table table-striped table-hover table-condensed" style="table-layout: fixed;">
		<tbody id="<?=ont_monitor_escape($widgetkey)?>">
			<?=ont_monitor_widget_content()?>
		</tbody>
	</table>
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
