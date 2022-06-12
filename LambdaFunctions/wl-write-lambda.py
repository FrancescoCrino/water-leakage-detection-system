import boto3
import time
import json
import datetime
import base64


dynamodb = boto3.resource('dynamodb')
lambd = boto3.client('lambda')
table = dynamodb.Table('wlTable')

def get_timestamp():
    ts = time.time()
    return str(ts)
  
def get_id_scan():
  response = table.scan(  TableName = 'wlTable')
  id = len(response['Items']) + 1
  return id
  
def get_sensor_data(event):
  sensor_data = json.dumps(str(event).replace("'", '"'))
  return sensor_data

def puItem_cbm_table(event):
    response = table.put_item(
        Item= {
            'id': get_id_scan(),
            'timestamp': get_timestamp(),
            'sensorsData': str(event).replace("'", '"')
        }
    )

def lambda_handler(event, context):
  puItem_cbm_table(event)

  
