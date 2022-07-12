<?php

function cicci_order_sync_admin_form() {
	// Ensure plain-text password is deleted..
	variable_del('cicci_order_sync_sftp_password');
	$form['sftp'] = array(
		'#type' => 'fieldset',
		'#title' => 'SFTP Settings',
		'#description' => t('Configure the SFTP server the order sync module interacts with.'),
	);
	$form['sftp']['cicci_order_sync_sftp_private_key_fid'] = array(
		'#type' => 'managed_file',
		'#title' => t('SFTP Private Key'),
		'#description' => t('The uploaded file will be used to connect to the SFTP server specified below.'),
		'#default_value' => variable_get('cicci_order_sync_sftp_private_key_fid', ''),
		'#upload_location' => 'public://private_keys/',
		'#upload_validators' => array(
			'file_validate_extensions' => array('ppk'),
		),
	);
	$form['sftp']['cicci_order_sync_sftp_server'] = array(
		'#type' => 'textfield',
		'#title' => t('SFTP Server'),
		'#description' => t('The IP or Hostname of the SFTP server'),
		'#default_value' => variable_get('cicci_order_sync_sftp_server','')
	);
	$form['sftp']['cicci_order_sync_sftp_username'] = array(
		'#type' => 'textfield',
		'#title' => t('SFTP Username'),
		'#description' => t('The username to use when connecting to the SFTP server'),
		'#default_value' => variable_get('cicci_order_sync_sftp_username','')
	);
	$form['sftp']['cicci_order_sync_sftp_password'] = array(
		'#type' => 'password',
		'#title' => t('SFTP Password'),
		'#description' => t('The password to use when connecting to the SFTP server.  <em>Leave blank to keep password unchanged.</em>'),
		'#default_value' => variable_get('cicci_order_sync_sftp_password','')
	);
	$form['sftp']['cicci_order_sync_sftp_root'] = array(
		'#type' => 'textfield',
		'#title' => t('SFTP Root Directory'),
		'#description' => t('The folder on the SFTP server where the ack_pending, ack_errored, and ack_processed folders can be found.  Do not include trailing slash.'),
		'#default_value' => variable_get('cicci_order_sync_sftp_root','')
	);
	$form['#submit'][] = 'cicci_order_sync_admin_logs_submit';
	return system_settings_form($form);
}

function cicci_order_sync_admin_form_submit($form, $form_state) {
	if (isset($form_state['values']['cicci_order_sync_sftp_private_key_fid']) && $form_state['values']['cicci_order_sync_sftp_private_key_fid'] != '') {
  		$file = file_load($form_state['values']['cicci_order_sync_sftp_private_key_fid']);
  		$file->status = FILE_STATUS_PERMANENT;
  		file_save($file);
  		// Record that the module is using the file. 
		file_usage_add($file, 'cicci_order_sync', 'cicci_order_sync', 0); 
	}
	if (isset($form_state['values']['cicci_order_sync_sftp_password']) &&  $form_state['values']['cicci_order_sync_sftp_password'] != '') {
		// Delete password (stored automagically since admin form is sent through system_settings_form)
		// from variable, make new encoded variable
		variable_del('cicci_order_sync_sftp_password');
		variable_set('cicci_order_sync_sftp_password_encoded', base64_encode(CICCI_ORDER_SYNC_SFTP_SALT . $form_state['values']['cicci_order_sync_sftp_password']));
	}
}

function cicci_order_sync_admin_logs() {
	$query = db_select('cicci_order_sync_log','l')->extend('PagerDefault')->extend('TableSort');
	$header = array(
		'timestamp' => array('data' => 'Timestamp' , 'field' => 'l.timestamp', 'sort' => 'DESC'),
		'filename' => array('data' => 'Filename', 'field' => 'l.filename'),
		'order_id' => array('data' => 'Order ID', 'field' => 'l.order_id'),
		'stage' => array('data' => 'Stage', 'field' => 'l.stage'),
		'message' => array('data' => 'Message', 'field' => 'l.message'),
	);
	$filters = array();
	$query
		->fields('l')
		->limit(50);
	if (isset($_SESSION['cicci_order_sync_admin_filters']) && !empty($_SESSION['cicci_order_sync_admin_filters'])) {
		$filters = $_SESSION['cicci_order_sync_admin_filters'];
		foreach($_SESSION['cicci_order_sync_admin_filters'] as $key => $value) {
			$query->condition($key, $value);
		}
	}
	if (!isset($filters['order_id'])) {
		$filters['order_id'] = '';
	}
	$query->orderByHeader($header);
	$logs = $query
			->execute()
			->fetchAllAssoc('log_id');
	$rows = array();
	foreach($logs as $log) {
		if ($log->order_id == -1) {
			$log->order_id = NULL;
		}
		$rows[] = array(
			'timestamp' => date('g:i:s A n/j/Y',$log->timestamp),
			'filename' => basename($log->filename),
			'order_id' => $log->order_id,
			'stage' => $log->stage,
			'message' => '<pre style="font-size:12px">' . $log->message . '</pre>',
			'#attributes' => array(
				'class' => array($log->stage),
			),
		);
	}
	$form['download_pending'] = array(
		'#type' => 'submit',
		'#value' => t('Get Pending from SFTP'),
	);
	$form['push_processed'] = array(
		'#type' => 'submit',
		'#value' => t('Push Processed to SFTP'),
	);
	$form['queue_pending'] = array(
		'#type' => 'submit',
		'#value' => t('Queue Pending Acknowledgements'),
	);
	$form['clear_queue'] = array(
		'#type' => 'submit',
		'#value' => t('Clear queue'),
	);


	$last_run = variable_get('cicci_order_sync_last_run', 0);
	$next_run = $last_run + 10800;
	$last_run = date('l, F j, Y g:i A T',$last_run);
	$next_run = date('l, F j, Y g:i A T',$next_run);
	$form['cicci_order_sync_info'] = array(
		'#type' => 'fieldset',
		'#title' => t('Order Sync Items'),
		'#description' => t('<strong>Last ran on @lastrun.<br />Next run scheduled for @nextrun.</strong><br /><br />These files are waiting to be processed or uploaded.  They exist at @dirname', array('@lastrun' => $last_run, '@nextrun' => $next_run,'@dirname' => CICCI_ORDER_SYNC_ROOT_DIR)),
		'#collapsible' => TRUE,
		'#collapsed' => FALSE,
	);
	$acknowledgement_states = _cicci_order_sync_order_acknowledgement_states();
	$finished_dir = str_replace('cicci_order_sync','cicci_order_sync_finished', CICCI_ORDER_SYNC_ROOT_DIR);
	foreach($acknowledgement_states as $state_name) {
		$acknowledgements = _cicci_order_sync_order_acknowledgements($state_name);
		$form['cicci_order_sync_info'][$state_name] = array(
			'#markup' => t('There are <strong>@num</strong> files in the <em>ack_@name</em> directory.',array('@num' => count($acknowledgements), '@name' => $state_name)),
			'#prefix' => '<p>',
			'#suffix' => '</p>',
		);	
	}
	$form['cicci_order_sync_finished_info'] = array(
		'#type' => 'fieldset',
		'#title' => t('Finished Order Sync Items'),
		'#description' => t('These files have been completely processed by the system and exist for posterity.  They exist at @dirname', array('@dirname' => $finished_dir)),
		'#collapsible' => TRUE,
		'#collapsed' => TRUE,
	);
	foreach($acknowledgement_states as $state_name) {
		if ($state_name != 'pending') {
			$acknowledgements = _cicci_order_sync_order_acknowledgements($state_name, TRUE);
			$form['cicci_order_sync_finished_info'][$state_name] = array(
				'#markup' => t('There are <strong>@num</strong> files in the <em>ack_@name</em> directory.',array('@num' => count($acknowledgements), '@name' => $state_name)),
				'#prefix' => '<p>',
				'#suffix' => '</p>',
			);	
		}
	}
	
	$form['clear_log'] = array(
		'#type' => 'submit',
		'#value' => t('Clear All Log Messages'),
		'#prefix' => '<div>',
		'#suffix' => '</div>',
	);
	$form['filter_order_id'] = array(
		'#type' => 'textfield',
		'#title' => 'Filter by Order ID',
		'#size' => 10,
		'#default_value' => $filters['order_id'],
	);
	$form['filter'] = array(
		'#type' => 'button',
		'#value' => t('Filter'),
		'#executes_submit_callback' => TRUE,
	);	
	$form['filter_reset'] = array(
		'#type' => 'submit',
		'#value' => t('Reset'),
		'#executes_submit_callback' => TRUE,
	);		
	$form['log_table'] = array(
		'#type' => 'tableselect',
		'#header' => $header,
		'#options' => $rows,
		'#empty' => t('No events have been logged yet.'),
		'#sticky' => FALSE,
	);
	$form['pager'] = array('#markup' => theme('pager'));
	$form['#attached']['css'][] = drupal_get_path('module','cicci_order_sync') . '/css/cicci_order_sync.css';
	return $form;
}

function cicci_order_sync_admin_logs_submit($form, &$form_state) {
	
	switch($form_state['triggering_element']['#value']) {
		case 'Clear All Log Messages':
			db_delete('cicci_order_sync_log')->execute();
			drupal_set_message('All log messages cleared');
			break;
		case 'Queue Pending Acknowledgements':
			_cicci_order_sync_queue_pending();
			drupal_set_message('Any pending acknowledgement is now in Cron Queue.  Run Cron to process.');
			break;
		case 'Clear queue':
		$queue = DrupalQueue::get('sync_order');
		$count = $queue->numberOfItems();
		$queue->deleteQueue();
		drupal_set_message(t('@items were removed from the queue.',array('@items' => $count)));
		break;
		case 'Get Pending from SFTP':
			cicci_order_sync_get_remote_files('ack_pending');
			drupal_set_message('Pending acknowledgements on the SFTP have been downloaded to this server.');
			break;
		case 'Push Processed to SFTP':
			// Check processed and put remotely
			$processed = _cicci_order_sync_order_acknowledgements('processed');
			if ($processed) {
				cicci_order_sync_put_remote_files($processed, 'ack_processed');
				drupal_set_message('Processed acknowledgements have been pushed to the SFTP server.');
			}
			// Check errored and put remotely
			$errored = _cicci_order_sync_order_acknowledgements('errored');
			if ($errored) {
				cicci_order_sync_put_remote_files($errored, 'ack_errored');
				drupal_set_message('Errored acknowledgements have been pushed to the SFTP server.');
			}	
			break;
		case 'Filter':
			$_SESSION['cicci_order_sync_admin_filters'] = array(
				'order_id' => $form_state['values']['filter_order_id'],
			);
			$form_state['rebuild'] = TRUE;
		break;
		case 'Reset':
			unset($_SESSION['cicci_order_sync_admin_filters']);
			$form_state['rebuild'] = TRUE;
		break;
	}
}

function cicci_order_sync_admin_flags() {
	module_load_include('inc', 'cicci_order_sync','cicci_order_sync.rules');

	// Build table header
	$header = array(
		'order_id' => array('data' => 'Order ID', 'field' => 'order_id', 'sort' => 'desc'),
		'customer_number' => array('data' => 'Customer Number', 'field' => 'customer_number'),
		'created' => array('data' => 'Date Created', 'field' => 'created'),
		'changed' => array('data' => 'Date Changed', 'field' => 'changed'),
		'total' => array('data' => 'Total', 'field' => 'total'),
		'balance' => array('data' => 'Balance', 'field' => 'balance'),
	);

	// Create EFQ for commerce_orders
	$query = new EntityFieldQuery();
	$query = $query->entityCondition('entity_type', 'commerce_order');

	// Flag to filter out completed orders
	$show_completed = isset($_GET['show_completed']) ? $_GET['show_completed'] : FALSE;
	if (!$show_completed) {
		$query = $query->propertyCondition('status', 'completed', '!=');
	}

	$result = $query->execute();

	$flagged = 0;
	$rows = array();
	
	// Create table rows from result
	if (!empty($result)) {
		$order_ids = array_keys($result['commerce_order']);
		$orders = commerce_order_load_multiple($order_ids);

		foreach($orders as $order) {
			if (cicci_order_sync_order_is_flagged($order)) {
				$flagged++;
				$balance = commerce_payment_order_balance($order);
				$order_wrapper = entity_metadata_wrapper('commerce_order', $order);
				$total = $order_wrapper->commerce_order_total->value();
				$customer = user_load($order->uid);
				$user_wrapper = entity_metadata_wrapper('user', $customer);
				$customer_number = $user_wrapper->field_customer_number->value();
				$rows[$order->order_id] = array(
					'order_id' => $order->order_id,
					'customer_number' => $customer_number,
					'created' => date('l, F j, Y g:i A T', $order->created),
					'changed' => date('l, F j, Y g:i A T', $order->changed),
					'total' => commerce_currency_format($total['amount'], $total['currency_code']),
					'balance' => commerce_currency_format($balance['amount'], $balance['currency_code']),
				);
			}
		}
	}

	// Sort data 
	$sort = tablesort_get_sort($header);
	$order = tablesort_get_order($header);

	if ($order && isset($order['sql'])) {
	  $sql = $order['sql'];
	  switch($sort) {
	    case 'desc':
	      uasort($rows, function($a, $b) use ($sql) {
	        switch($sql) {
	          case 'order_id':
	          case 'customer_number':
	            return -1 * strcmp($a[$sql], $b[$sql]);
	          break;
	          case 'created':
  	          case 'changed':
	            return strtotime($a[$sql]) > strtotime($b[$sql]) ? -1 : 1;
	          break;
	          case 'total':
	          case 'balance':
	          	$fmt = numfmt_create( 'en_US', NumberFormatter::CURRENCY );
	          	$c = 'USD';
	          	$decimalA = numfmt_parse_currency($fmt, $a[$sql], $c);
	          	$decimalB = numfmt_parse_currency($fmt, $b[$sql], $c);
	          	return $decimalA > $decimalB ? -1 : 1;
	          break;
	          default:
	            return $a[$sql] > $b[$sql] ? -1 : 1;
	        }
	        
	      });
	    break;
	    case 'asc':
	      uasort($rows, function($a, $b) use ($sql) {
	        switch($sql) {
	          case 'order_id':
	          case 'customer_number':
	            return strcmp($a[$sql], $b[$sql]);
	          break;
	          case 'created':
  	          case 'changed':
	            return strtotime($a[$sql]) > strtotime($b[$sql]) ? 1 : -1;
	          break;
	          case 'total':
	          case 'balance':
	          	$fmt = numfmt_create( 'en_US', NumberFormatter::CURRENCY );
	          	$c = 'USD';
	          	$decimalA = numfmt_parse_currency($fmt, $a[$sql], $c);
	          	$decimalB = numfmt_parse_currency($fmt, $b[$sql], $c);
	          	return $decimalA > $decimalB ? 1 : -1;
	          break;
	          default:
	            return $a[$sql] > $b[$sql] ? 1 : -1;
	        }
	      });
	    break;
	  }
	}

	// Display information 
	$form['info'] = array('#markup' => t('<p>There are @count flagged orders.</p>',array('@count' => $flagged)));

	// Render tableselect
	$form['log_table'] = array(
		'#type' => 'tableselect',
		'#header' => $header,
		'#options' => $rows,
		'#empty' => t('No orders have been flagged.'),
		'#sticky' => FALSE,
	);

	// Add buttons
	$form['submit_top'] = array(
		'#type' => 'submit',
		'#value' => t('Release Selected Orders'),
		'#weight' => -10,
	);
	if ($show_completed) {
		$form['hide_completed'] = array(
			'#type' => 'submit', 
			'#weight' => -5,
			'#value' => t('Hide Completed Orders')
		);
	}
	else {
		$form['show_completed'] = array(
		'#type' => 'submit', 
		'#weight' => -5,
		'#value' => t('Show Completed Orders, too')
	);
	}
	$form['submit'] = array(
		'#type' => 'submit',
		'#value' => t('Release Selected Orders'),
		'#weight' => 10,
	);
	return $form;
}

function cicci_order_sync_admin_flags_submit($form, &$form_state) {
	switch($form_state['triggering_element']['#value']) {
		case 'Release Selected Orders':
			$released = 0;
			// Iterate through tableselect
			foreach($form_state['values']['log_table'] as $order_id => $is_checked) {
				if ($is_checked !== 0) {
					$order = commerce_order_load($order_id);
					// Remove the key from the data array
					unset($order->data['cicci_order_sync_flagged']);
					// Save order so flag is gone
					commerce_order_save($order);
					// Invoke rules event to handle order completion
					rules_invoke_event('commerce_checkout_complete', $order);
					$released++;
				}
			}
			if ($released) { 
				drupal_set_message(
					t('@total @subjectverb and set to completed.',
						array(
							'@total' => $released, 
							'@subjectverb' => format_plural($released, 'order was released', 'orders were released')
						)
					)
				); 
			}
			else {
				drupal_set_message(t('No flagged orders were selected to be released.'), 'error');
			}
		break;
		case 'Show Completed Orders, too':
			drupal_goto(current_path(), array('query' => array('show_completed' => 1)));
		break;
		case 'Hide Completed Orders':
			drupal_goto(current_path(), array('query' => array('show_completed' => 0)));
		break;		
	}
}