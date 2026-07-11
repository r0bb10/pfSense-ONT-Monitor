<?php

$priv_list['page-services-ont-monitor'] = [];
$priv_list['page-services-ont-monitor']['name'] = 'WebCfg - Services: ONT Monitor';
$priv_list['page-services-ont-monitor']['descr'] = 'Allow access to ONT Monitor settings and status';
$priv_list['page-services-ont-monitor']['match'] = [];
$priv_list['page-services-ont-monitor']['match'][] = 'pkg_edit.php?xml=ont-monitor.xml*';
$priv_list['page-services-ont-monitor']['match'][] = 'status_ont_monitor.php*';
