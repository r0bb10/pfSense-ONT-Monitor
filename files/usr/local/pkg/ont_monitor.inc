<?php

require_once('functions.inc');
require_once('globals.inc');

const ONT_MONITOR_DEFAULTS = [
	'host' => '192.168.1.1',
	'source_address' => '192.168.1.2',
	'username' => 'admin',
	'password' => 'admin',
	'refresh_seconds' => '60',
];

const ONT_MONITOR_RUNTIME_STATE_FILE = '/var/db/ont-monitor/state.json';
const ONT_MONITOR_FETCH_ERROR_LOG_INTERVAL = 300;
const ONT_MONITOR_ERROR_COUNTER_LOG_INTERVAL = 300;

function ont_monitor_install() {
	if (!config_get_path('installedpackages/ontmonitor/config/0')) {
		config_set_path('installedpackages/ontmonitor/config/0', ONT_MONITOR_DEFAULTS);
		write_config('Installed ONT Monitor package.');
	}
}

function ont_monitor_deinstall() {
	config_del_path('installedpackages/ontmonitor');
	write_config('Removed ONT Monitor package configuration.');
}

function ont_monitor_settings() {
	$settings = config_get_path('installedpackages/ontmonitor/config/0', []);
	$settings = array_merge(ONT_MONITOR_DEFAULTS, is_array($settings) ? $settings : []);

	foreach (['host', 'source_address'] as $field) {
		if (!filter_var($settings[$field], FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)) {
			$settings[$field] = ONT_MONITOR_DEFAULTS[$field];
		}
	}
	foreach (['username', 'password'] as $field) {
		if (!is_string($settings[$field]) || $settings[$field] === '') {
			$settings[$field] = ONT_MONITOR_DEFAULTS[$field];
		}
	}

	$settings['refresh_seconds'] = max(30, min(3600, (int)$settings['refresh_seconds']));
	return $settings;
}

function ont_monitor_escape($value) {
	return htmlspecialchars((string)$value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function ont_monitor_request($url, $settings, $options = []) {
	$ch = curl_init($url);
	$base_options = [
		CURLOPT_RETURNTRANSFER => true,
		CURLOPT_CONNECTTIMEOUT => 3,
		CURLOPT_TIMEOUT => 8,
		CURLOPT_SSL_VERIFYPEER => false,
		CURLOPT_SSL_VERIFYHOST => 0,
		CURLOPT_INTERFACE => $settings['source_address'],
		CURLOPT_USERAGENT => 'pfSense-ONT-Monitor/1.0',
	];
	curl_setopt_array($ch, array_replace($base_options, $options));
	$body = curl_exec($ch);
	$error = $body === false ? curl_error($ch) : '';
	$status = (int)curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
	curl_close($ch);
	return [$body, $status, $error];
}

function ont_monitor_js_value($body, $name) {
	$pattern = '/(?:var\\s+)?' . preg_quote($name, '/') . '\\s*=\\s*"([^"]*)";/';
	if (preg_match_all($pattern, $body, $matches) !== false && !empty($matches[1])) {
		return html_entity_decode(end($matches[1]), ENT_QUOTES | ENT_HTML5, 'UTF-8');
	}
	return null;
}

function ont_monitor_format_number($value, $precision = 2) {
	return number_format((float)$value, $precision, '.', '');
}

function ont_monitor_runtime_state() {
	if (!is_readable(ONT_MONITOR_RUNTIME_STATE_FILE)) {
		return [];
	}

	$state = json_decode((string)file_get_contents(ONT_MONITOR_RUNTIME_STATE_FILE), true);
	return is_array($state) ? $state : [];
}

function ont_monitor_save_runtime_state($state) {
	$directory = dirname(ONT_MONITOR_RUNTIME_STATE_FILE);
	if (!is_dir($directory) && !mkdir($directory, 0700, true)) {
		return;
	}

	file_put_contents(ONT_MONITOR_RUNTIME_STATE_FILE, json_encode($state), LOCK_EX);
}

function ont_monitor_log($message, $priority = LOG_NOTICE) {
	syslog($priority, 'ONT Monitor: ' . $message);
}

function ont_monitor_value($value) {
	return $value === null || $value === '' ? '-' : (string)$value;
}

function ont_monitor_log_fetch_failure($message) {
	$state = ont_monitor_runtime_state();
	$now = time();
	$last_logged = (int)($state['fetch_error_logged_at'] ?? 0);
	if (($state['fetch_error'] ?? null) !== $message || $now - $last_logged >= ONT_MONITOR_FETCH_ERROR_LOG_INTERVAL) {
		ont_monitor_log('fetch failed: ' . $message, LOG_ERR);
		$state['fetch_error_logged_at'] = $now;
	}
	$state['fetch_error'] = $message;
	ont_monitor_save_runtime_state($state);
}

function ont_monitor_fetch_error($message) {
	ont_monitor_log_fetch_failure($message);
	return ['error' => $message];
}

function ont_monitor_log_status($data) {
	$state = ont_monitor_runtime_state();
	$changed = false;

	if (!empty($state['fetch_error'])) {
		ont_monitor_log('fetch recovered', LOG_NOTICE);
		unset($state['fetch_error'], $state['fetch_error_logged_at']);
		$changed = true;
	}

	$gpon_state = ont_monitor_value($data['gpon']['state']);
	$gpon_label = ont_monitor_value($data['gpon']['state_label']);
	$gpon_description = $gpon_state . ' ' . $gpon_label;
	if (!array_key_exists('gpon_state', $state)) {
		ont_monitor_log('GPON state: ' . $gpon_description);
		$state['gpon_state'] = $gpon_state;
		$state['gpon_label'] = $gpon_label;
		$changed = true;
	} elseif ($state['gpon_state'] !== $gpon_state || $state['gpon_label'] !== $gpon_label) {
		ont_monitor_log('GPON state changed: ' . $state['gpon_state'] . ' ' . $state['gpon_label'] . ' -> ' . $gpon_description);
		$state['gpon_state'] = $gpon_state;
		$state['gpon_label'] = $gpon_label;
		$changed = true;
	}

	$ethernet_status = ont_monitor_value($data['ethernet']['status']);
	if (!array_key_exists('ethernet_status', $state)) {
		ont_monitor_log('Ethernet link: ' . $ethernet_status);
		$state['ethernet_status'] = $ethernet_status;
		$changed = true;
	} elseif ($state['ethernet_status'] !== $ethernet_status) {
		ont_monitor_log('Ethernet link changed: ' . $state['ethernet_status'] . ' -> ' . $ethernet_status);
		$state['ethernet_status'] = $ethernet_status;
		$changed = true;
	}

	$rx_errors = ont_monitor_value($data['ethernet']['rx_errors']);
	$tx_errors = ont_monitor_value($data['ethernet']['tx_errors']);
	$errors_changed = !array_key_exists('rx_errors', $state) || $state['rx_errors'] !== $rx_errors || $state['tx_errors'] !== $tx_errors;
	$has_errors = (is_numeric($rx_errors) && (int)$rx_errors > 0) || (is_numeric($tx_errors) && (int)$tx_errors > 0);
	$had_errors = (is_numeric($state['rx_errors'] ?? null) && (int)$state['rx_errors'] > 0) || (is_numeric($state['tx_errors'] ?? null) && (int)$state['tx_errors'] > 0);
	$last_error_log = (int)($state['errors_logged_at'] ?? 0);
	if ($errors_changed && $has_errors && (!$had_errors || time() - $last_error_log >= ONT_MONITOR_ERROR_COUNTER_LOG_INTERVAL)) {
		ont_monitor_log('Ethernet errors: RX ' . $rx_errors . ', TX ' . $tx_errors, LOG_WARNING);
		$state['errors_logged_at'] = time();
		$changed = true;
	} elseif ($errors_changed && $had_errors && !$has_errors) {
		ont_monitor_log('Ethernet errors cleared', LOG_NOTICE);
		unset($state['errors_logged_at']);
		$changed = true;
	}
	if ($errors_changed) {
		$state['rx_errors'] = $rx_errors;
		$state['tx_errors'] = $tx_errors;
		$changed = true;
	}

	if ($changed) {
		ont_monitor_save_runtime_state($state);
	}
}

function ont_monitor_gpon_state($state) {
	$states = [
		'1' => ['code' => 'O1', 'label' => 'Initial state', 'severity' => 'warning'],
		'2' => ['code' => 'O2', 'label' => 'Standby state', 'severity' => 'warning'],
		'3' => ['code' => 'O3', 'label' => 'Serial Number state', 'severity' => 'warning'],
		'4' => ['code' => 'O4', 'label' => 'Ranging state', 'severity' => 'warning'],
		'5' => ['code' => 'O5', 'label' => 'Operational', 'severity' => 'success'],
		'6' => ['code' => 'O6', 'label' => 'POPUP state', 'severity' => 'danger'],
		'7' => ['code' => 'O7', 'label' => 'Emergency Stop state', 'severity' => 'danger'],
	];

	return $states[(string)$state] ?? ['code' => null, 'label' => 'Unknown state', 'severity' => 'danger'];
}

function ont_monitor_ethernet_status($link_up, $link_mode) {
	if ($link_up !== '1') {
		return 'Down';
	}

	$link_modes = [
		1 => '10base-T <full-duplex>',
		2 => '100base-T <full-duplex>',
		3 => '1000base-T <full-duplex>',
		6 => '2.5Gbase-T <full-duplex>',
		17 => '10base-T <half-duplex>',
		18 => '100base-T <half-duplex>',
		19 => '1000base-T <half-duplex>',
		21 => '2.5Gbase-T <half-duplex>',
	];

	return $link_modes[(int)$link_mode] ?? 'Up';
}

function ont_monitor_fetch() {
	if (!function_exists('curl_init')) {
		return ont_monitor_fetch_error('PHP cURL support is unavailable.');
	}

	$settings = ont_monitor_settings();
	$base_url = 'https://' . $settings['host'];
	$session_cookie = '';
	$nonce = bin2hex(random_bytes(16));
	$login_headers = [
		'Referer: ' . $base_url . '/login.html',
		'Origin: ' . $base_url,
		'X-Requested-With: XMLHttpRequest',
		'Content-Type: application/x-www-form-urlencoded; charset=UTF-8',
	];

	$header_callback = static function ($handle, $header) use (&$session_cookie) {
		if (stripos($header, 'Set-Cookie:') === 0 && strpos($header, '-goahead-session-=') !== false) {
			$session_cookie = strtok(trim(substr($header, strlen('Set-Cookie:'))), ';');
		}
		return strlen($header);
	};

	[$login_body, $login_status, $login_error] = ont_monitor_request(
		$base_url . '/goform/LoginForm',
		$settings,
		[
			CURLOPT_POST => true,
			CURLOPT_POSTFIELDS => http_build_query([
				'cmt' => hash('sha256', $settings['username'] . '97s' . $settings['password']),
				'nonce' => $nonce,
			]),
			CURLOPT_HTTPHEADER => $login_headers,
			CURLOPT_HEADERFUNCTION => $header_callback,
		]
	);

	if ($login_error !== '') {
		return ont_monitor_fetch_error('ONT login request failed: ' . $login_error);
	}
	if ($login_status !== 200 || strpos((string)$login_body, 'loginStatus = ok') === false || $session_cookie === '') {
		return ont_monitor_fetch_error('ONT login failed. Verify the source address and credentials.');
	}

	$pages = [];
	foreach (['devinfo', 'gponinfo', 'userinfo'] as $page) {
		[$body, $status, $error] = ont_monitor_request(
			$base_url . '/' . $page . '.html',
			$settings,
			[CURLOPT_COOKIE => $session_cookie]
		);
		if ($error !== '' || $status !== 200 || $body === false) {
			return ont_monitor_fetch_error('Unable to read ONT ' . $page . ' data.');
		}
		$pages[$page] = $body;
	}

	ont_monitor_request($base_url . '/goform/LogoutForm', $settings, [
		CURLOPT_POST => true,
		CURLOPT_POSTFIELDS => '',
		CURLOPT_COOKIE => $session_cookie,
	]);

	$gpon_state = ont_monitor_js_value($pages['gponinfo'], 'ponStaIndex');
	$tx_raw = ont_monitor_js_value($pages['gponinfo'], 'txpower');
	$rx_raw = ont_monitor_js_value($pages['gponinfo'], 'rxpower');
	$voltage_raw = ont_monitor_js_value($pages['gponinfo'], 'voltage');
	$current_raw = ont_monitor_js_value($pages['gponinfo'], 'current');
	$temperature_raw = ont_monitor_js_value($pages['gponinfo'], 'temperature');
	$link_up = ont_monitor_js_value($pages['userinfo'], 'linkUp');
	$link_mode = ont_monitor_js_value($pages['userinfo'], 'mode');
	$gpon = ont_monitor_gpon_state($gpon_state);

	$data = [
		'updated' => date('c'),
		'device' => [
			'model' => ont_monitor_js_value($pages['devinfo'], 'Model'),
			'hardware' => ont_monitor_js_value($pages['devinfo'], 'HardwareVer'),
			'software' => ont_monitor_js_value($pages['devinfo'], 'SoftwareVer'),
			'bootloader' => ont_monitor_js_value($pages['devinfo'], 'BootLoaderVer'),
			'pon_serial' => ont_monitor_js_value($pages['devinfo'], 'PonSerialNum'),
		],
		'gpon' => [
			'state' => $gpon['code'],
			'state_label' => $gpon['label'],
			'state_severity' => $gpon['severity'],
			'rx_power_dbm' => is_numeric($rx_raw) ? ont_monitor_format_number(((float)$rx_raw / 500) - 40) : null,
			'tx_power_dbm' => is_numeric($tx_raw) ? ont_monitor_format_number(((float)$tx_raw / 500) - 30) : null,
			'voltage_v' => is_numeric($voltage_raw) ? ont_monitor_format_number((float)$voltage_raw / 50) : null,
			'bias_ma' => is_numeric($current_raw) ? ont_monitor_format_number((float)$current_raw / 1000) : null,
			'temperature_c' => is_numeric($temperature_raw) ? (string)intdiv((int)$temperature_raw, 256) : null,
		],
		'ethernet' => [
			'mac' => ont_monitor_js_value($pages['userinfo'], 'Mac'),
			'status' => ont_monitor_ethernet_status($link_up, $link_mode),
			'rx_errors' => ont_monitor_js_value($pages['userinfo'], 'rxErrs'),
			'tx_errors' => ont_monitor_js_value($pages['userinfo'], 'txErrs'),
		],
	];
	ont_monitor_log_status($data);
	return $data;
}
