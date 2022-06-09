from django.contrib.auth import get_user_model
from django.http import JsonResponse
from django.shortcuts import render
from django.views.generic import View

from rest_framework.views import APIView
from rest_framework.response import Response

import requests
import json
from datetime import datetime

User = get_user_model()

class HomeView(View):
    def get(self, request, *args, **kwargs):
        return render(request, 'charts.html', {"customers": 10})



def get_data():
    data = {"water":[], "mov":[], "leak":[], "date":[]}
    labels = []
    response = requests.get("https://f748j01iwd.execute-api.us-east-1.amazonaws.com/wld/data")
    response_json = response.json()
    body = response_json['body']
    data_raw = body.replace("]]", "").replace("[[", "").split("], [")
    for d in data_raw:
        sd_json = json.loads(json.loads(str(d))['sensorsData'])
        data["water"].append(sd_json["water"])
        data["mov"].append(sd_json["movement"])
        data["leak"].append(sd_json["leakage"])
        timestamp = json.loads(str(d))['timestamp']
        date_object = datetime.fromtimestamp(float(timestamp))
        form_date = str(date_object).split(".")[0]
        data["date"].append(form_date)
        labels.append(form_date.split(" ")[1])
    return data, labels


class ChartData(APIView):
    authentication_classes = []
    permission_classes = []
    def get(self, request, format=None):
        data, labels = get_data()
        ret_data = {"data":data, "labels":labels}
        return Response(ret_data)


