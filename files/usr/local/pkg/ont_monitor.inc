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
	$pattern = '/var\\s+' . preg_quote($name, '/') . '\\s*=\\s*"([^"]*)";/' ;
	return preg_match($pattern, $body, $matches) === 1 ? html_entity_decode($matches[1], ENT_QUOTES | ENT_HTML5, 'UTF-8') : null;
}

function ont_monitor_format_number($value, $precision = 2) {
	return number_format((float)$value, $precision, '.', '');
}

function ont_monitor_fetch() {
	if (!function_exists('curl_init')) {
		return ['error' => 'PHP cURL support is unavailable.'];
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
		return ['error' => 'ONT login request failed: ' . $login_error];
	}
	if ($login_status !== 200 || strpos((string)$login_body, 'loginStatus = ok') === false || $session_cookie === '') {
		return ['error' => 'ONT login failed. Verify the source address and credentials.'];
	}

	$pages = [];
	foreach (['devinfo', 'gponinfo', 'userinfo'] as $page) {
		[$body, $status, $error] = ont_monitor_request(
			$base_url . '/' . $page . '.html',
			$settings,
			[CURLOPT_COOKIE => $session_cookie]
		);
		if ($error !== '' || $status !== 200 || $body === false) {
			return ['error' => 'Unable to read ONT ' . $page . ' data.'];
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
	$link_modes = [
		1 => '10 Mbps full duplex',
		2 => '100 Mbps full duplex',
		3 => '1 Gbps full duplex',
		6 => '2.5 Gbps full duplex',
		17 => '10 Mbps half duplex',
		18 => '100 Mbps half duplex',
		19 => '1 Gbps half duplex',
		21 => '2.5 Gbps half duplex',
	];

	return [
		'updated' => date('c'),
		'device' => [
			'model' => ont_monitor_js_value($pages['devinfo'], 'Model'),
			'hardware' => ont_monitor_js_value($pages['devinfo'], 'HardwareVer'),
			'software' => ont_monitor_js_value($pages['devinfo'], 'SoftwareVer'),
			'bootloader' => ont_monitor_js_value($pages['devinfo'], 'BootLoaderVer'),
			'pon_serial' => ont_monitor_js_value($pages['devinfo'], 'PonSerialNum'),
		],
		'gpon' => [
			'state' => $gpon_state === null ? null : 'O' . $gpon_state,
			'rx_power_dbm' => is_numeric($rx_raw) ? ont_monitor_format_number(((float)$rx_raw / 500) - 40) : null,
			'tx_power_dbm' => is_numeric($tx_raw) ? ont_monitor_format_number(((float)$tx_raw / 500) - 30) : null,
			'voltage_v' => is_numeric($voltage_raw) ? ont_monitor_format_number((float)$voltage_raw / 50) : null,
			'bias_ma' => is_numeric($current_raw) ? ont_monitor_format_number((float)$current_raw / 1000) : null,
			'temperature_c' => is_numeric($temperature_raw) ? (string)intdiv((int)$temperature_raw, 256) : null,
		],
		'ethernet' => [
			'mac' => ont_monitor_js_value($pages['userinfo'], 'Mac'),
			'status' => $link_up === '1' ? ($link_modes[(int)$link_mode] ?? 'Up') : 'Down',
			'rx_errors' => ont_monitor_js_value($pages['userinfo'], 'rxErrs'),
			'tx_errors' => ont_monitor_js_value($pages['userinfo'], 'txErrs'),
		],
	];
}
