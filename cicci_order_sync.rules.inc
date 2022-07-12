<?php

/**
 * Implements hook_rules_condition_info().
 */
function cicci_order_sync_rules_condition_info() {
	$conditions = array();
	$conditions['cicci_order_sync_order_is_from_ack'] = array(
    'label' => t('Order from Acknowldgement'),
    'parameter' => array(
      'commerce_order' => array(
        'type' => 'commerce_order',
        'label' => t('Order'),
        'description' => t('The order to check against.'),
      ),
    ),
    'group' => t('Commerce Order'),
    'callbacks' => array(
      'execute' => 'cicci_order_sync_order_is_from_acknowledgement',
    ),
  );
$conditions['cicci_order_sync_order_is_flagged'] = array(
    'label' => t('Order Flagged from Order Sync'),
    'parameter' => array(
      'commerce_order' => array(
        'type' => 'commerce_order',
        'label' => t('Order'),
        'description' => t('The order to check against.'),
      ),
    ),
    'group' => t('Commerce Order'),
    'callbacks' => array(
      'execute' => 'cicci_order_sync_order_is_flagged',
    ),
  );  
  return $conditions;
}



/**
 * Condition callback: compares the selected payment method for an order.
 */
function cicci_order_sync_order_is_from_acknowledgement($order) {
  return isset($order->data) && isset($order->data['cicci_order_sync_invoked']);
}

function cicci_order_sync_order_is_flagged($order) {
  return isset($order->data) && isset($order->data['cicci_order_sync_flagged']) && $order->data['cicci_order_sync_flagged'] === TRUE;
}

// function cicci_order_sync_default_rules_configuration_alter(&$configs) {
// 	dpm($configs);
// }