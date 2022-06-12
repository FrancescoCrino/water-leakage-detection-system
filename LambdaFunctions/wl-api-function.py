import json
import boto3
import datetime
import base64
from boto3.dynamodb.conditions import Key, Attr
from decimal import Decimal

dynamodb = boto3.resource('dynamodb')
lambd = boto3.client('lambda')
table = dynamodb.Table('wlTable')

def get_dim_table():
  response = table.scan(TableName = 'wlTable')
  dim_tab = len(response['Items']) 
  return dim_tab

def get_last_records():
    nitems = get_dim_table()
    
    ids_to_ret = set()
    for i in range(0, 15):
        id_to_ret = nitems - i
        ids_to_ret.add(id_to_ret)
    
    results = []
    
    for id_ret in ids_to_ret:
        response = table.query(KeyConditionExpression=Key('id').eq(id_ret))
        results.append(response['Items'])

    return results

def lambda_handler(event, context):
    results = get_last_records()
    return {
        'statusCode': 200,
        'body': json.dumps(results, cls=DecimalEncoder)
    }


class DecimalEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, Decimal):
            return str(obj)
        return json.JSONEncoder.default(self, obj)