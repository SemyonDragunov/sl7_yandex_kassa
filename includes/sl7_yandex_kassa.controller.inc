<?php

/**
 * @author Semyon Dragunov <sam.dragunov@gmail.com>
 * https://github.com/SemyonDragunov
 */

class SL7YandexKassaPaymentController extends SL7PaymentController
{
  /**
   * Handles "checkOrder" and "paymentAviso" requests.
   * @param array $request payment parameters
   */
  public function processRequest($request) {
    // If the MD5 checking fails, respond with "1" error code
    if (!$this->checkMD5($request)) {
      $response = $this->buildResponse($request['action'], $request['invoiceId'], 1);
      $this->sendResponse($response);
    }

    $response = null;
    if ($request['action'] == 'checkOrder') {
      $response = $this->checkOrder($request);
    } else {
      $response = $this->paymentAviso($request);
    }

    $this->sendResponse($response);
  }

  public function sendResponse($response) {
    header('Content-Type: application/xml');
    echo $response;
    drupal_exit();
  }

  /**
   * CheckOrder request processing. We suppose there are no item with price less
   * than 100 rubles in the shop.
   * @param  array $request payment parameters
   * @return string         prepared XML response
   */
  private function checkOrder($request) {
    $response = null;
    if ($request['orderSumAmount'] < 100) {
      $response = $this->buildResponse($request['action'], $request['invoiceId'], 100, "The amount should be more than 100 rubles.");
    } else {
      $response = $this->buildResponse($request['action'], $request['invoiceId'], 0);
    }
    return $response;
  }

  /**
   * PaymentAviso request processing.
   * @param  array $request payment parameters
   * @return string prepared response in XML format
   */
  private function paymentAviso($request) {
    return $this->buildResponse($request['action'], $request['invoiceId'], 0);
  }

  /**
   * Checking the MD5 sign.
   * @param  array $request payment parameters
   * @return bool true if MD5 hash is correct
   */
  private function checkMD5($request) {
    $str = $request['action'] . ";" .
      $request['orderSumAmount'] . ";" . $request['orderSumCurrencyPaycash'] . ";" .
      $request['orderSumBankPaycash'] . ";" . $request['shopId'] . ";" .
      $request['invoiceId'] . ";" . $request['customerNumber'] . ";" . variable_get('sl7_yandex_kassa_shoppassword', '');
    $md5 = strtoupper(md5($str));
    if ($md5 != strtoupper($request['md5'])) {
      return false;
    }
    return true;
  }

  public function createMD5($params = array()) {
    if (!empty($params)) {
      $output = '';
      $output .= $params['action'];
      $output .= ';' . $params['orderSumAmount'];
      $output .= ';' . $params['orderSumCurrencyPaycash'];
      $output .= ';' . $params['orderSumBankPaycash'];
      $output .= ';' . $params['shopId'];
      $output .= ';' . $params['invoiceId'];
      $output .= ';' . $params['customerNumber'];
      $md5 = md5($output . ';' . variable_get('sl7_yandex_kassa_shoppassword', ''));
      return drupal_strtoupper($md5);
    }
    else {
      return '';
    }
  }

  /**
   * Building XML response.
   * @param  string $functionName  "checkOrder" or "paymentAviso" string
   * @param  string $invoiceId     transaction number
   * @param  string $result_code   result code
   * @param  string $message       error message. May be null.
   * @return string                prepared XML response
   */
  private function buildResponse($functionName, $invoiceId, $result_code, $message = null) {
    try {
      $response = '<?xml version="1.0" encoding="UTF-8"?><' . $functionName . 'Response performedDatetime="' . date('c') .
        '" code="' . $result_code . '" ' . ($message != null ? 'message="' . $message . '"' : "") . ' invoiceId="' . $invoiceId . '" shopId="' . variable_get('sl7_yandex_kassa_shopid') . '"/>';
      return $response;
    } catch (\Exception $e) {
      watchdog(__FUNCTION__, $e, array(), WATCHDOG_ERROR);
    }
    return null;
  }

  private function _query($order) {
    global $user;

    $shopId = variable_get('sl7_yandex_kassa_shopid');
    $scid = variable_get('sl7_yandex_kassa_scid');
    $shopPassword = variable_get('sl7_yandex_kassa_shoppassword');
    $sum = sl7_order_get_sum($order->oid);

    $query = array(
      'shopId' => $shopId,
      'scid' => $scid,
      'shopPassword' => $shopPassword,
      'sum' => $sum,
      'customerNumber' => $order->recipient,
      'orderNumber' => $order->oid,
      'paymentType' => '',
    );

    if (variable_get('sl7_yandex_kassa_success_select') == 'own') {
      $query['shopSuccessURL'] = variable_get('sl7_yandex_kassa_success_url');
    }
    if (variable_get('sl7_yandex_kassa_fail_select') == 'own') {
      $query['shopFailURL'] = variable_get('sl7_yandex_kassa_fail_url');
    }
    if (variable_get('sl7_yandex_kassa_default_select') == 'own') {
      $query['shopDefaultUrl'] = variable_get('sl7_yandex_kassa_default_url');
    }

    if ($user->mail) $query['cps_email'] = $user->mail;

    return $query;
  }

  public function external($order, $ajax = FALSE) {
    $order = is_object($order) ? $order : sl7_order_load($order);
    if (!$this->checkup()) exit;

    $url = sl7_yandex_kassa_server_url(variable_get('sl7_yandex_kassa_server_name'));

    if ($ajax) {
      ctools_include('ajax');
      ctools_add_js('ajax-responder');
      $commands[] = ctools_ajax_command_redirect($url, 0, array('query' => $this->_query($order)));
      print ajax_render($commands);
      exit;
    }
    else {
      drupal_goto($url, array('query' => $this->_query($order)));
    }
  }
}